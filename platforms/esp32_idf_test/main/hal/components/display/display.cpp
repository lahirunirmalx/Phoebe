/**
 * @file display.cpp
 * @brief See display.h. Owns the SSD1306 init + flush helpers; the actual
 *        I2C bus is brought up by HalEsp32::i2c_init() so this just uses
 *        the existing port.
 */
#include "display.h"
#include "../../hal_config.h"
#include <cstdint>
#include <cstring>
#include <driver/i2c.h>
#include <esp_log.h>

static const char* TAG = "ssd1306";

namespace ssd1306 {

constexpr int W = HAL_SCREEN_WIDTH;
constexpr int H = HAL_SCREEN_HEIGHT;
constexpr int PAGES = H / 8;
constexpr size_t FB_SIZE = (size_t)W * PAGES;

static uint8_t s_fb[FB_SIZE];

static esp_err_t i2c_write(const uint8_t* buf, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (HAL_OLED_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, buf, len, true);
    i2c_master_stop(cmd);
    esp_err_t rc = i2c_master_cmd_begin(HAL_I2C_BUS_PORT_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return rc;
}

static esp_err_t send_cmd(uint8_t c)
{
    uint8_t buf[2] = {0x00, c}; // 0x00 = command, single byte follows
    return i2c_write(buf, 2);
}

static esp_err_t send_cmds(const uint8_t* cmds, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        esp_err_t rc = send_cmd(cmds[i]);
        if (rc != ESP_OK) return rc;
    }
    return ESP_OK;
}

void chip_init()
{
    static const uint8_t init_cmds[] = {
        0xAE,             // display off
        0xD5, 0x80,       // clock divide ratio / osc freq
        0xA8, 0x3F,       // multiplex (64 - 1)
        0xD3, 0x00,       // display offset
        0x40,             // start line 0
        0x8D, 0x14,       // charge pump on
        0x20, 0x00,       // memory addressing = horizontal
        0xA1,             // segment remap (col 127 -> SEG0)
        0xC8,             // COM scan dir remapped
        0xDA, 0x12,       // COM pins hw config
        0x81, 0xCF,       // contrast
        0xD9, 0xF1,       // pre-charge period
        0xDB, 0x40,       // VCOMH deselect level
        0xA4,             // entire display on follows RAM
        0xA6,             // non-inverted
        0x2E,             // deactivate scroll
        0xAF,             // display on
    };
    esp_err_t rc = send_cmds(init_cmds, sizeof(init_cmds));
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "init seq failed: %d", rc);
        return;
    }
    std::memset(s_fb, 0, FB_SIZE);
    ESP_LOGI(TAG, "SSD1306 ready @ 0x%02X on SDA=%d SCL=%d", HAL_OLED_I2C_ADDR,
             HAL_PIN_IMU_I2C_BUS_SDA, HAL_PIN_IMU_I2C_BUS_SCL);
}

// Threshold-convert an RGB565 row-major framebuffer into the SSD1306's
// page-column layout. Any non-zero pixel = on.
void blit_rgb565(const uint16_t* src)
{
    std::memset(s_fb, 0, FB_SIZE);
    for (int y = 0; y < H; ++y) {
        const int page = y >> 3;
        const uint8_t bit = 1u << (y & 7);
        const uint16_t* row = src + (size_t)y * W;
        uint8_t* page_row = s_fb + (size_t)page * W;
        for (int x = 0; x < W; ++x) {
            if (row[x]) page_row[x] |= bit;
        }
    }
}

void flush()
{
    static const uint8_t addr_cmds[] = {
        0x21, 0x00, 0x7F, // col start, end
        0x22, 0x00, 0x07, // page start, end
    };
    send_cmds(addr_cmds, sizeof(addr_cmds));

    constexpr size_t CHUNK = 32;
    uint8_t buf[CHUNK + 1];
    buf[0] = 0x40; // data stream
    for (size_t off = 0; off < FB_SIZE; off += CHUNK) {
        size_t n = (FB_SIZE - off) < CHUNK ? (FB_SIZE - off) : CHUNK;
        std::memcpy(&buf[1], s_fb + off, n);
        i2c_write(buf, n + 1);
    }
}

} // namespace ssd1306

void DisplaySsd1306::init()
{
    // LGFX sprite setup expected by mooncake's display abstraction. We
    // never push pixels through it ourselves -- the LVGL flush callback
    // (in hal_esp32.cpp) writes directly to the SSD1306 framebuffer.
    setColorDepth(lgfx::color_depth_t::rgb565_nonswapped);
    createSprite(HAL_SCREEN_WIDTH, HAL_SCREEN_HEIGHT);
}

void DisplaySsd1306::push_buffer_to_display(void* buffer)
{
    ssd1306::blit_rgb565(static_cast<const uint16_t*>(buffer));
    ssd1306::flush();
}

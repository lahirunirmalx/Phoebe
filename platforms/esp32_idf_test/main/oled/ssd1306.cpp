/**
 * @file ssd1306.cpp
 * @brief See ssd1306.h. Uses the legacy esp-idf i2c driver to stay
 *        compatible with both IDF v4.x and v5.x.
 */
#include "ssd1306.h"
#include "font5x8.h"
#include "../hal_config.h"

#include <cstring>
#include <driver/i2c.h>
#include <esp_log.h>

static const char* TAG = "ssd1306";

namespace {

static uint8_t s_fb[oled::FB_SIZE];

esp_err_t i2c_write(const uint8_t* buf, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (HAL_OLED_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, buf, len, true);
    i2c_master_stop(cmd);
    esp_err_t rc = i2c_master_cmd_begin((i2c_port_t)HAL_I2C_PORT, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return rc;
}

esp_err_t send_cmd(uint8_t c)
{
    uint8_t buf[2] = {0x00, c}; // 0x00 = command stream, single byte follows
    return i2c_write(buf, 2);
}

esp_err_t send_cmds(const uint8_t* cmds, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        esp_err_t rc = send_cmd(cmds[i]);
        if (rc != ESP_OK) return rc;
    }
    return ESP_OK;
}

} // namespace

namespace oled {

bool init()
{
    // I2C master config
    i2c_config_t cfg = {};
    cfg.mode = I2C_MODE_MASTER;
    cfg.sda_io_num = HAL_PIN_OLED_SDA;
    cfg.scl_io_num = HAL_PIN_OLED_SCL;
    cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    cfg.master.clk_speed = HAL_I2C_FREQ_HZ;

    esp_err_t rc = i2c_param_config((i2c_port_t)HAL_I2C_PORT, &cfg);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %d", rc);
        return false;
    }
    rc = i2c_driver_install((i2c_port_t)HAL_I2C_PORT, cfg.mode, 0, 0, 0);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %d", rc);
        return false;
    }

    // SSD1306 init sequence (Adafruit/Heltec compatible 128x64).
    static const uint8_t init_cmds[] = {
        0xAE,             // display off
        0xD5, 0x80,       // set display clock divide ratio / osc freq
        0xA8, 0x3F,       // set multiplex (64 - 1)
        0xD3, 0x00,       // set display offset
        0x40,             // set start line to 0
        0x8D, 0x14,       // charge pump on
        0x20, 0x00,       // memory addressing mode = horizontal
        0xA1,             // segment remap (col 127 -> SEG0)
        0xC8,             // COM scan dir remapped
        0xDA, 0x12,       // COM pins hw config
        0x81, 0xCF,       // contrast
        0xD9, 0xF1,       // pre-charge period
        0xDB, 0x40,       // VCOMH deselect level
        0xA4,             // entire display on follows RAM
        0xA6,             // normal (non-inverted) display
        0x2E,             // deactivate scroll
        0xAF,             // display on
    };
    rc = send_cmds(init_cmds, sizeof(init_cmds));
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "init seq failed: %d", rc);
        return false;
    }

    clear();
    flush();
    ESP_LOGI(TAG, "SSD1306 ready @ 0x%02X on SDA=%d SCL=%d", HAL_OLED_I2C_ADDR,
             HAL_PIN_OLED_SDA, HAL_PIN_OLED_SCL);
    return true;
}

void clear()
{
    std::memset(s_fb, 0, FB_SIZE);
}

void set_pixel(int x, int y, bool on)
{
    if ((unsigned)x >= W || (unsigned)y >= H) return;
    const int page = y >> 3;
    const uint8_t bit = 1u << (y & 7);
    if (on) {
        s_fb[page * W + x] |= bit;
    } else {
        s_fb[page * W + x] &= ~bit;
    }
}

void fill_rect(int x, int y, int w, int h, bool on)
{
    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            set_pixel(x + dx, y + dy, on);
        }
    }
}

int draw_char(int x, int y, char ch, bool on)
{
    // Draws only the lit pixels of the glyph; background is left as-is so
    // callers can compose text on top of other content without erasing it.
    const uint8_t* glyph = font5x8::glyph(ch);
    for (int col = 0; col < 5; ++col) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 8; ++row) {
            if ((bits >> row) & 1) {
                set_pixel(x + col, y + row, on);
            }
        }
    }
    return x + 6;
}

int draw_text(int x, int y, const char* s, bool on)
{
    while (*s) {
        x = draw_char(x, y, *s, on);
        ++s;
    }
    return x;
}

void flush()
{
    // Set column range 0..127, page range 0..7, then stream the whole fb.
    static const uint8_t addr_cmds[] = {
        0x21, 0x00, 0x7F, // col start, end
        0x22, 0x00, 0x07, // page start, end
    };
    send_cmds(addr_cmds, sizeof(addr_cmds));

    // Send data in chunks. Each chunk is prefixed with 0x40 (data stream).
    constexpr size_t CHUNK = 32;
    uint8_t buf[CHUNK + 1];
    buf[0] = 0x40;
    for (size_t off = 0; off < FB_SIZE; off += CHUNK) {
        size_t n = (FB_SIZE - off) < CHUNK ? (FB_SIZE - off) : CHUNK;
        std::memcpy(&buf[1], s_fb + off, n);
        i2c_write(buf, n + 1);
    }
}

} // namespace oled

#include "st7789.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <cstring>

// ============================================================================
//  RECOMMENDED ESP32 <-> ST7789 WIRING  (edit here if you rewire)
//  Chosen to avoid strapping pins (0/2/5/12/15), flash pins (6-11) and
//  input-only pins (34-39), and to stay clear of the touch pad on GPIO32.
//
//    ST7789 pin     ESP32 GPIO
//    -----------    ----------
//    VCC        ->  3V3
//    GND        ->  GND
//    SCL / CLK  ->  GPIO18   (SPI clock)
//    SDA / MOSI ->  GPIO23   (SPI data)
//    RES / RST  ->  GPIO4
//    DC         ->  GPIO16
//    CS         ->  GPIO22   (if your module has no CS pin, tie it low on the
//                             board and set PIN_CS to -1 below)
//    BLK / BL   ->  GPIO21   (backlight; or tie straight to 3V3)
// ============================================================================
// SCLK/MOSI confirmed from the board's ESPHome config (HSPI hardware bus).
// ---- Freenove ESP32 Mini TV (FNK0112) -- pins from the board's ESPHome config.
#define PIN_SCLK 14    // HSPI clock          (ESPHome clk_pin)
#define PIN_MOSI 13    // HSPI data           (ESPHome mosi_pin)
#define PIN_DC    2    // data/command        (ESPHome dc_pin)
#define PIN_CS   15    // chip select         (ESPHome cs_pin; board may also tie CS low)
#define PIN_RST  -1    // no reset pin wired  (ESPHome reset_pin commented out -> SW reset)
#define PIN_BL   19    // backlight, ACTIVE-LOW    (ESPHome ledc output, inverted)
#define PIN_VDD  21    // panel power-enable, ACTIVE-LOW -- MUST pull LOW or the panel is unpowered

#define LCD_W    240
#define LCD_H    240
#define LCD_HOST SPI2_HOST

static const char* TAG = "st7789";
static spi_device_handle_t s_spi;

// SPI pre-transfer callback: drive DC from the transaction's `user` flag.
static void IRAM_ATTR dc_pre_cb(spi_transaction_t* t) {
    gpio_set_level((gpio_num_t)PIN_DC, (int)(intptr_t)t->user);
}

static void wr_cmd(uint8_t c) {
    spi_transaction_t t = {};
    t.length    = 8;
    t.tx_buffer = &c;
    t.user      = (void*)0;            // DC low = command
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
}

static void wr_data(const uint8_t* d, int len) {
    if (len == 0) return;
    spi_transaction_t t = {};
    t.length    = 8 * len;
    t.tx_buffer = d;
    t.user      = (void*)1;            // DC high = data
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
}

static void wr_data1(uint8_t d) { wr_data(&d, 1); }

void st7789_backlight(bool on) { gpio_set_level((gpio_num_t)PIN_BL, on ? 0 : 1); } // active-low

static void set_window(int x0, int y0, int x1, int y1) {
    uint8_t buf[4];
    wr_cmd(0x2A);                                              // CASET (columns)
    buf[0] = x0 >> 8; buf[1] = x0; buf[2] = x1 >> 8; buf[3] = x1; wr_data(buf, 4);
    wr_cmd(0x2B);                                              // RASET (rows)
    buf[0] = y0 >> 8; buf[1] = y0; buf[2] = y1 >> 8; buf[3] = y1; wr_data(buf, 4);
    wr_cmd(0x2C);                                              // RAMWR
}

void st7789_init(void) {
    // Outputs: DC, backlight (active-low), and the panel VDD power-enable
    // (active-low). Reset pin only if one is actually wired.
    gpio_config_t io = {};
    io.mode         = GPIO_MODE_OUTPUT;
    io.pin_bit_mask = (1ULL << PIN_DC) | (1ULL << PIN_BL) | (1ULL << PIN_VDD);
#if PIN_RST >= 0
    io.pin_bit_mask |= (1ULL << PIN_RST);
#endif
    ESP_ERROR_CHECK(gpio_config(&io));

    gpio_set_level((gpio_num_t)PIN_BL, 1);    // backlight OFF (active-low) until init done
    gpio_set_level((gpio_num_t)PIN_VDD, 0);   // VDD enable is active-low -> power the panel ON
    vTaskDelay(pdMS_TO_TICKS(50));            // let the rail settle

    spi_bus_config_t bus = {};
    bus.mosi_io_num     = PIN_MOSI;
    bus.miso_io_num     = -1;
    bus.sclk_io_num     = PIN_SCLK;
    bus.quadwp_io_num   = -1;
    bus.quadhd_io_num   = -1;
    bus.max_transfer_sz = LCD_W * 2 * 40;                      // a few rows per xfer
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev = {};
    // The FNK0112's ESPHome config notes "original device uses 20MHz ... does
    // not work at 80MHz". 20 MHz is safe and plenty for solid fills.
    dev.clock_speed_hz = 20 * 1000 * 1000;                     // 20 MHz
    dev.mode           = 0;                                    // SPI mode 0
    dev.spics_io_num   = PIN_CS;
    dev.queue_size     = 7;
    dev.pre_cb         = dc_pre_cb;
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &dev, &s_spi));

#if PIN_RST >= 0
    // Hardware reset (only if a reset pin is wired)
    gpio_set_level((gpio_num_t)PIN_RST, 0); vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level((gpio_num_t)PIN_RST, 1); vTaskDelay(pdMS_TO_TICKS(120));
#endif

    wr_cmd(0x01); vTaskDelay(pdMS_TO_TICKS(150));              // SWRESET
    wr_cmd(0x11); vTaskDelay(pdMS_TO_TICKS(120));              // SLPOUT
    wr_cmd(0x3A); wr_data1(0x55);                             // COLMOD = 16-bit/pixel
    wr_cmd(0x36); wr_data1(0x00);                            // MADCTL = RGB, default orient
    wr_cmd(0x21);                                            // INVON (IPS panels need inversion)
    wr_cmd(0x13);                                            // NORON
    wr_cmd(0x29); vTaskDelay(pdMS_TO_TICKS(10));             // DISPON
    ESP_LOGI(TAG, "init done (SCLK=%d MOSI=%d DC=%d RST=%d CS=%d BL=%d)",
             PIN_SCLK, PIN_MOSI, PIN_DC, PIN_RST, PIN_CS, PIN_BL);
}

void st7789_fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    set_window(x, y, x + w - 1, y + h - 1);

    static uint16_t line[LCD_W];
    const uint16_t swapped = (uint16_t)((color >> 8) | (color << 8)); // ST7789 wants big-endian
    const int cols = (w > LCD_W) ? LCD_W : w;
    for (int i = 0; i < cols; ++i) line[i] = swapped;
    for (int row = 0; row < h; ++row) wr_data((const uint8_t*)line, cols * 2);
}

void st7789_fill(uint16_t color) { st7789_fill_rect(0, 0, LCD_W, LCD_H, color); }

/**
 * @file st7789.cpp
 * @brief ST7789 240x240 SPI backend. Ported from platforms/tft_touch_test,
 *        parameterised by the pins in hal_config.h. The DC line is driven from
 *        the SPI pre-transfer callback; commands go out with DC low, data high.
 */
#include "st7789.h"
#include "../../hal_config.h"
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

static const char* TAG = "st7789";
static spi_device_handle_t s_spi = nullptr;

namespace st7789 {

static void IRAM_ATTR dc_pre_cb(spi_transaction_t* t)
{
    gpio_set_level((gpio_num_t)HAL_PIN_LCD_DC, (int)(intptr_t)t->user);
}

static void wr_cmd(uint8_t c)
{
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &c;
    t.user = (void*)0; // DC low = command
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
}

static void wr_data(const uint8_t* d, int len)
{
    if (len == 0) return;
    spi_transaction_t t = {};
    t.length = 8 * len;
    t.tx_buffer = d;
    t.user = (void*)1; // DC high = data
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
}

static void wr_data1(uint8_t d) { wr_data(&d, 1); }

static void set_window(int x0, int y0, int x1, int y1)
{
    uint8_t buf[4];
    wr_cmd(0x2A); // CASET
    buf[0] = x0 >> 8; buf[1] = x0; buf[2] = x1 >> 8; buf[3] = x1; wr_data(buf, 4);
    wr_cmd(0x2B); // RASET
    buf[0] = y0 >> 8; buf[1] = y0; buf[2] = y1 >> 8; buf[3] = y1; wr_data(buf, 4);
    wr_cmd(0x2C); // RAMWR
}

void chip_init()
{
    // DC is an output; RST handled below only if a pin is wired.
    gpio_config_t io = {};
    io.mode = GPIO_MODE_OUTPUT;
    io.pin_bit_mask = (1ULL << HAL_PIN_LCD_DC);
#if HAL_PIN_LCD_RST >= 0
    io.pin_bit_mask |= (1ULL << HAL_PIN_LCD_RST);
#endif
    ESP_ERROR_CHECK(gpio_config(&io));

    spi_bus_config_t bus = {};
    bus.mosi_io_num = HAL_PIN_LCD_MOSI;
    bus.miso_io_num = -1;
    bus.sclk_io_num = HAL_PIN_LCD_SCLK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = HAL_SCREEN_WIDTH * HAL_SCREEN_HEIGHT * 2;
    ESP_ERROR_CHECK(spi_bus_initialize(HAL_LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev = {};
    dev.clock_speed_hz = HAL_LCD_SPI_HZ;
    dev.mode = 0;
    dev.spics_io_num = HAL_PIN_LCD_CS;
    dev.queue_size = 7;
    dev.pre_cb = dc_pre_cb;
    ESP_ERROR_CHECK(spi_bus_add_device(HAL_LCD_SPI_HOST, &dev, &s_spi));

#if HAL_PIN_LCD_RST >= 0
    gpio_set_level((gpio_num_t)HAL_PIN_LCD_RST, 0); vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level((gpio_num_t)HAL_PIN_LCD_RST, 1); vTaskDelay(pdMS_TO_TICKS(120));
#endif

    wr_cmd(0x01); vTaskDelay(pdMS_TO_TICKS(150)); // SWRESET
    wr_cmd(0x11); vTaskDelay(pdMS_TO_TICKS(120)); // SLPOUT
    wr_cmd(0x3A); wr_data1(0x55);                 // COLMOD 16bpp
    wr_cmd(0x36); wr_data1(0x00);                 // MADCTL RGB
    wr_cmd(0x21);                                 // INVON (IPS / board uses inverted colors)
    wr_cmd(0x13);                                 // NORON
    wr_cmd(0x29); vTaskDelay(pdMS_TO_TICKS(10));  // DISPON
    ESP_LOGI(TAG, "init done (SCLK=%d MOSI=%d DC=%d CS=%d)",
             HAL_PIN_LCD_SCLK, HAL_PIN_LCD_MOSI, HAL_PIN_LCD_DC, HAL_PIN_LCD_CS);
}

void blit(int x1, int y1, int x2, int y2, const uint16_t* data)
{
    const int w = x2 - x1 + 1;
    const int h = y2 - y1 + 1;
    if (w <= 0 || h <= 0) return;
    set_window(x1, y1, x2, y2);
    wr_data(reinterpret_cast<const uint8_t*>(data), w * h * 2);
}

} // namespace st7789

/**
 * @file sharpe_mlcd.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-02
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "sharpe_mlcd.h"
// https://www.sharpsde.com/fileadmin/products/Displays/Specs/LS013B7DH05_25Jan24_Spec_LD-2023X06.pdf
// https://github.com/adafruit/Adafruit_SHARP_Memory_Display/blob/master/Adafruit_SharpMem.h

#define SHARPMEM_BIT_WRITECMD (0x01) // 0x80 in LSB format
#define SHARPMEM_BIT_VCOM     (0x02) // 0x40 in LSB format
#define SHARPMEM_BIT_CLEAR    (0x04) // 0x20 in LSB format

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) \
    {                       \
        int16_t t = a;      \
        a = b;              \
        b = t;              \
    }
#endif
#ifndef _swap_uint16_t
#define _swap_uint16_t(a, b) \
    {                        \
        uint16_t t = a;      \
        a = b;               \
        b = t;               \
    }
#endif

// 1<<n is a costly operation on AVR -- table usu. smaller & faster
static const uint8_t PROGMEM set[] = {1, 2, 4, 8, 16, 32, 64, 128},
                             clr[] = {(uint8_t)~1,  (uint8_t)~2,  (uint8_t)~4,  (uint8_t)~8,
                                      (uint8_t)~16, (uint8_t)~32, (uint8_t)~64, (uint8_t)~128};

bool SharpeMlcd::init()
{
    // SPI
    SPI.setFrequency(_config.spi_clk_freq);
    SPI.setBitOrder(SPI_LSBFIRST);
    SPI.begin(_config.pin_sclk, -1, _config.pin_si, -1);
    _spi_settings = new SPISettings(_config.spi_clk_freq, SPI_LSBFIRST, SPI_MODE0);

    // Set the vcom bit to a defined state
    _sharpmem_vcom = SHARPMEM_BIT_VCOM;

    // Framebuffer
    _sharpmem_buffer = new uint8_t[(_config.screen_width * _config.screen_height) / 8];

    // Chip select pin
    gpio_reset_pin((gpio_num_t)_config.pin_scs);
    gpio_set_direction((gpio_num_t)_config.pin_scs, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode((gpio_num_t)_config.pin_scs, GPIO_PULLUP_PULLDOWN);
    gpio_set_level((gpio_num_t)_config.pin_scs, 0);
    // lgfx::delay(100);

    return true;
}

void SharpeMlcd::clearScreen()
{
    clearBuffer();

    SPI.beginTransaction(*_spi_settings);
    gpio_set_level((gpio_num_t)_config.pin_scs, 1);

    // Send the clear screen command rather than doing a HW refresh (quicker)
    uint8_t data[2] = {(uint8_t)(_sharpmem_vcom | SHARPMEM_BIT_CLEAR), 0x00};
    SPI.transfer(data, 2);

    toggle_vcom();

    gpio_set_level((gpio_num_t)_config.pin_scs, 0);

    SPI.endTransaction();
}

void SharpeMlcd::refreshScreen()
{
    uint16_t i, currentline;

    SPI.beginTransaction(*_spi_settings);

    // Send the write command
    gpio_set_level((gpio_num_t)_config.pin_scs, 1);

    SPI.transfer(_sharpmem_vcom | SHARPMEM_BIT_WRITECMD);

    toggle_vcom();

    uint8_t bytes_per_line = _config.screen_width / 8;
    uint16_t totalbytes = (_config.screen_width * _config.screen_height) / 8;

    for (i = 0; i < totalbytes; i += bytes_per_line) {
        uint8_t line[bytes_per_line + 2];

        // Send address byte
        currentline = ((i + 1) / (_config.screen_width / 8)) + 1;
        line[0] = currentline;

        // copy over this line
        memcpy(line + 1, _sharpmem_buffer + i, bytes_per_line);

        // Send end of line
        line[bytes_per_line + 1] = 0x00;
        // send it!
        SPI.transfer(line, bytes_per_line + 2);
    }

    // Send another trailing 8 bits for the last line
    SPI.transfer(0x00);

    gpio_set_level((gpio_num_t)_config.pin_scs, 0);

    SPI.endTransaction();
}

void SharpeMlcd::clearBuffer()
{
    memset(_sharpmem_buffer, 0xff, (_config.screen_width * _config.screen_height) / 8);
}

void SharpeMlcd::fillBufferBlack()
{
    memset(_sharpmem_buffer, 0x00, (_config.screen_width * _config.screen_height) / 8);
}

void SharpeMlcd::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if ((x < 0) || (x >= _config.screen_width) || (y < 0) || (y >= _config.screen_height))
        return;
    drawPixelPreclipped(x, y, color);
}

uint_fast8_t SharpeMlcd::rgb565_to_grayscale(uint_fast16_t rgb565)
{
    // Extract the G component
    uint_fast8_t g = (rgb565 >> 5) & 0x3F; // G: 6 bits

    // Expand G to 8 bits
    return (g * 255) / 63; // Convert to an 8-bit grayscale value
}

void SharpeMlcd::drawPixelPreclipped(uint_fast16_t x, uint_fast16_t y, uint_fast16_t color)
{
    switch (_config.rotation) {
        case 1:
            _swap_int16_t(x, y);
            x = _config.screen_width - 1 - x;
            break;
        case 2:
            x = _config.screen_width - 1 - x;
            y = _config.screen_height - 1 - y;
            break;
        case 3:
            _swap_int16_t(x, y);
            y = _config.screen_height - 1 - y;
            break;
    }

    // if (color) {
    //     _sharpmem_buffer[(y * _config.screen_width + x) / 8] |= pgm_read_byte(&set[x & 7]);
    // } else {
    //     _sharpmem_buffer[(y * _config.screen_width + x) / 8] &= pgm_read_byte(&clr[x & 7]);
    // }

    if (rgb565_to_grayscale(color) > 168) {
        _sharpmem_buffer[(y * _config.screen_width + x) / 8] |= pgm_read_byte(&set[x & 7]);
    } else {
        _sharpmem_buffer[(y * _config.screen_width + x) / 8] &= pgm_read_byte(&clr[x & 7]);
    }
}

void SharpeMlcd::copyBuffer(uint16_t* colors)
{
    if (!colors) {
        return;
    }

    for (int y = 0; y < _config.screen_height; y++) {
        for (int x = 0; x < _config.screen_width; x++) {
            drawPixelPreclipped(x, y, colors[y * _config.screen_width + x]);
        }
    }
}

void SharpeMlcd::toggle_vcom()
{
    do {
        _sharpmem_vcom = _sharpmem_vcom ? 0x00 : SHARPMEM_BIT_VCOM;
    } while (0);
}

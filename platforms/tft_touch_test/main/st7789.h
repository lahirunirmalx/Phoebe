/**
 * @file st7789.h
 * @brief Minimal ST7789 240x240 SPI driver for the phoebe pin-discovery test.
 *        Write-only (no MISO). Just enough to fill the panel with solid colors
 *        so we can confirm the display is alive and wired correctly.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void st7789_init(void);
void st7789_backlight(bool on);
void st7789_fill(uint16_t color);                          /* whole screen      */
void st7789_fill_rect(int x, int y, int w, int h, uint16_t color);

#ifdef __cplusplus
}
#endif

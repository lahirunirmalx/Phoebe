/**
 * @file st7789.h
 * @brief Minimal ST7789 240x240 SPI driver for the Freenove Mini TV.
 *        Write-only panel; LVGL renders RGB565 and the flush callback pushes
 *        the dirty area here. Pins come from hal_config.h.
 *
 *        Assumes the panel VDD rail is already powered (the backlight component
 *        owns the active-low GPIO21 VDD-enable and asserts it before init).
 */
#pragma once
#include <cstdint>

namespace st7789 {

// Bring up the SPI bus + device and run the ST7789 init sequence.
void chip_init();

// Push a rectangular block to the panel. `data` is RGB565 in the panel's
// native (big-endian) byte order, row-major, covering the inclusive rect
// [x1..x2] x [y1..y2]. The LVGL flush callback byte-swaps before calling.
void blit(int x1, int y1, int x2, int y2, const uint16_t* data);

} // namespace st7789

/**
 * @file ssd1306.h
 * @brief Minimal SSD1306 128x64 I2C OLED driver. Maintains an in-RAM
 *        framebuffer (8 pages x 128 bytes); call flush() to push it out.
 *
 *        Layout matches the chip's GDDRAM: each byte is 8 vertical pixels
 *        in a column (LSB on top). Page = y >> 3, bit = y & 7.
 */
#pragma once
#include <cstdint>
#include <cstddef>

namespace oled {

constexpr int W = 128;
constexpr int H = 64;
constexpr int PAGES = H / 8;
constexpr size_t FB_SIZE = (size_t)W * PAGES;

// Initialise the I2C port and run the SSD1306 init sequence.
// Returns true on success.
bool init();

void clear();

// (x, y) origin top-left. No-op for out-of-bounds.
void set_pixel(int x, int y, bool on);

// Filled rectangle (x, y, w, h). Off-by-one safe.
void fill_rect(int x, int y, int w, int h, bool on);

// Render a single ASCII char (printable; non-printables drawn as space).
// Glyph is 5x8 column-major (see font5x8.cpp). dx advances by 6 px so the
// caller can chain calls without computing widths.
int draw_char(int x, int y, char ch, bool on);

// Convenience: render a NUL-terminated string left-to-right.
// Returns the cursor x after the last char.
int draw_text(int x, int y, const char* s, bool on);

// Push the framebuffer to the chip via I2C. Sends 8 page writes.
void flush();

} // namespace oled

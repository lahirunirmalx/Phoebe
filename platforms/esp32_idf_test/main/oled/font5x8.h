/**
 * @file font5x8.h
 * @brief Public-domain 5x8 ASCII bitmap font (column-major).
 *        Each glyph is 5 bytes; bit i of column byte = pixel at row i.
 */
#pragma once
#include <cstdint>

namespace font5x8 {

// Returns a pointer to a 5-byte glyph for `ch`. Non-printable falls back
// to a blank glyph.
const uint8_t* glyph(char ch);

} // namespace font5x8

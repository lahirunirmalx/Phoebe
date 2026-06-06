/**
 * @file display.h
 * @brief Display HAL backed by a 240x240 ST7789 TFT on SPI (Freenove Mini TV).
 *        LVGL renders RGB565 directly; the flush callback in hal_esp32.cpp
 *        byte-swaps and pushes the dirty area to the panel via st7789::blit.
 *        This class is the LGFX_Sprite shim mooncake's DisplayBase expects.
 */
#pragma once
#include <hal/hal.h>

class DisplaySt7789 : public hal_components::DisplayBase {
public:
    void init() override;
    void push_buffer_to_display(void* buffer) override;
};

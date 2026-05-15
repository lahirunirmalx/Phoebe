/**
 * @file display.h
 * @brief Display HAL backed by an SSD1306 128x64 monochrome OLED on I2C.
 *        LVGL renders RGB565; this component thresholds + transposes the
 *        rendered buffer onto the chip's page-column framebuffer.
 */
#pragma once
#include <hal/hal.h>

class DisplaySsd1306 : public hal_components::DisplayBase {
public:
    void init() override;
    void push_buffer_to_display(void* buffer) override;
};

/**
 * @file hal_esp32.h
 * @brief HAL for the Freenove ESP32 Mini TV (FNK0112): 240x240 ST7789 SPI
 *        display, capacitive touch T9, active-low backlight + panel VDD.
 */
#pragma once
#include <hal/hal.h>

class HalEsp32 : public HAL::HalBase {
public:
    std::string type() override
    {
        return "ESP32-MiniTV";
    }

    void init() override;

private:
    void lvgl_init();   // LVGL display + ST7789 flush + touch pointer indev
    void fs_init();
};

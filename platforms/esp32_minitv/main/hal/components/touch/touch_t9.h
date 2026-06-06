/**
 * @file touch_t9.h
 * @brief Capacitive touch on pad T9 (GPIO32) for the Freenove Mini TV.
 *
 * Legacy touch_pad v1 API (first-class on IDF v4.4). A finger shifts the
 * reading away from the idle baseline (direction-agnostic). The UI task calls
 * tick() each loop; the LVGL pointer indev reads pressed(); a >=3 s hold raises
 * a one-shot long-press used to open the captive portal.
 */
#pragma once
#include <cstdint>

namespace touch {

void init();                       // touch_pad bring-up + baseline calibration
void tick(std::uint32_t now_ms);   // sample + gesture timing (UI task)
bool pressed();                    // debounced current state (for LVGL indev)
bool take_long_press();            // true exactly once when a >=3 s hold completes

} // namespace touch

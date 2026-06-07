/**
 * @file touch_t9.cpp
 * @brief See touch_t9.h. Ported from platforms/tft_touch_test/main/touch_t9.cpp
 *        with gesture timing on top.
 */
#include "touch_t9.h"
#include "../../hal_config.h"
#include <driver/touch_pad.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mooncake_log.h>

namespace touch {

static const char* TAG = "touch";

static std::uint16_t s_baseline = 0;
static std::uint16_t s_delta = 1;      // +/- band that counts as a touch
static bool s_pressed = false;

// Gesture state
static bool s_press_active = false;    // physical hold in progress
static std::uint32_t s_press_start = 0;
static bool s_long_fired = false;      // long-press already reported this hold
static volatile bool s_long_pending = false;

static std::uint16_t read_raw()
{
    std::uint16_t v = 0;
    touch_pad_read_filtered(HAL_TOUCH_PAD_NUM, &v);
    return v;
}

void init()
{
    touch_pad_init();
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    touch_pad_config(HAL_TOUCH_PAD_NUM, 0);
    touch_pad_filter_start(10);

    vTaskDelay(pdMS_TO_TICKS(200)); // let the IIR filter settle (untouched)
    std::uint32_t sum = 0;
    const int n = 16;
    for (int i = 0; i < n; ++i) {
        sum += read_raw();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    s_baseline = (std::uint16_t)(sum / n);
    s_delta = s_baseline / 8; // ~12% deviation
    if (s_delta < 1) s_delta = 1;
    mclog::tagInfo(TAG, "T9/GPIO32 baseline={} delta={}", s_baseline, s_delta);
}

void tick(std::uint32_t now_ms)
{
    const std::uint16_t v = read_raw();
    const bool down = (v + s_delta < s_baseline) || (v > s_baseline + s_delta);

    if (down && !s_press_active) {
        // press begins
        s_press_active = true;
        s_press_start = now_ms;
        s_long_fired = false;
    } else if (!down && s_press_active) {
        // released
        s_press_active = false;
    }

    // Long-press: fire once when the hold passes the threshold.
    if (s_press_active && !s_long_fired &&
        (now_ms - s_press_start >= HAL_TOUCH_LONGPRESS_MS)) {
        s_long_fired = true;
        s_long_pending = true;
    }

    // Suppress the LVGL "pressed" report once a long-press has fired, so the
    // eventual release doesn't also register as a view-toggle tap.
    s_pressed = s_press_active && !s_long_fired;
}

bool pressed() { return s_pressed; }

bool take_long_press()
{
    if (!s_long_pending) return false;
    s_long_pending = false;
    return true;
}

} // namespace touch

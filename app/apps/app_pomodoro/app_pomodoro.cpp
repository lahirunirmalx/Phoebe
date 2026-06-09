/**
 * @file app_pomodoro.cpp
 * @brief See app_pomodoro.h.
 */
#include "app_pomodoro.h"

#include <cstdio>

using namespace ui;

namespace {
constexpr int POMO_WORK_SEC = 25 * 60;
constexpr int POMO_BREAK_SEC = 5 * 60;
}

AppPomodoro::AppPomodoro()
{
    setAppInfo().name = "pomodoro";
}

void AppPomodoro::build(lv_obj_t* root)
{
    _phase = lv_label_create(root);
    lv_obj_set_style_text_font(_phase, &lv_font_montserrat_24, 0);
    lv_label_set_text(_phase, "WORK");
    lv_obj_align(_phase, LV_ALIGN_TOP_MID, 0, 14);

    _arc = make_ring(root, 162, 10);
    lv_arc_set_bg_angles(_arc, 0, 360);
    lv_arc_set_rotation(_arc, 270);
    lv_obj_align(_arc, LV_ALIGN_CENTER, 0, 22);

    _time = lv_label_create(root);
    lv_obj_set_style_text_color(_time, COLOR_FG, 0);
    lv_obj_set_style_text_font(_time, &lv_font_montserrat_48, 0);
    lv_label_set_text(_time, "25:00");
    lv_obj_align(_time, LV_ALIGN_CENTER, 0, 22);

    // Fresh work session each time this app is opened.
    _work = true;
    _phase_start_ms = HAL::SysCtrl().millis();
}

void AppPomodoro::tick()
{
    if (!_time) return;
    const std::uint32_t now = HAL::SysCtrl().millis();
    const int phase_len = _work ? POMO_WORK_SEC : POMO_BREAK_SEC;
    int elapsed = (int)((now - _phase_start_ms) / 1000);

    if (elapsed >= phase_len) {
        _work = !_work;
        _phase_start_ms = now;
        elapsed = 0;
        HAL::Backlight().notify(hal_components::BacklightBase::Notify_LimitReached);
    }

    const int remaining = phase_len - elapsed;
    const lv_color_t c = _work ? COLOR_7D : COLOR_OK;
    lv_label_set_text(_phase, _work ? "WORK" : "BREAK");
    lv_obj_set_style_text_color(_phase, c, 0);

    char buf[24];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", remaining / 60, remaining % 60);
    lv_label_set_text(_time, buf);

    lv_arc_set_value(_arc, remaining * 100 / phase_len);
    lv_obj_set_style_arc_color(_arc, c, LV_PART_INDICATOR);
}

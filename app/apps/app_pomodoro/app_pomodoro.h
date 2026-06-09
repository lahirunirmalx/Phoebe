/**
 * @file app_pomodoro.h
 * @brief Pomodoro timer (25m work / 5m break). Self-contained; pulses the
 *        backlight at each phase change.
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"

class AppPomodoro : public ui::ScreenApp {
public:
    AppPomodoro();

protected:
    void build(lv_obj_t* root) override;
    void tick() override;

private:
    lv_obj_t* _phase = nullptr;
    lv_obj_t* _time = nullptr;
    lv_obj_t* _arc = nullptr;
    bool _work = true;
    std::uint32_t _phase_start_ms = 0;
};

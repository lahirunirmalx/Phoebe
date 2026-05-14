/**
 * @file app_claudemeter.h
 * @brief Claude API usage meter for the phoebe desktop sim.
 *
 * Mirrors the M5Cardputer-UserDemo app_claudemeter UI concept (5-hour and
 * 7-day usage bars, color-coded by threshold) but adapted for phoebe's
 * LVGL/SDL stack. Holds two views in a single app -- an analog clock and
 * the usage meter -- and toggles between them on screen click.
 *
 * The desktop sim has no real HTTP path to /usage, so this version drives
 * the bars with mock values that drift over time. Wiring it to a real
 * endpoint is straightforward later (ArduinoJson + libcurl in the HAL).
 */
#pragma once
#include <cstdint>
#include <mooncake.h>
#include <lvgl.h>

class AppClaudeMeter : public mooncake::AppAbility {
public:
    AppClaudeMeter();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    enum View { VIEW_CLOCK = 0, VIEW_METER = 1 };

    View _view = VIEW_CLOCK;
    std::uint32_t _last_tick_ms = 0;

    // Mock usage values; cycle between 0..100 so all three color bands
    // (green / orange / red) are visible during a single run.
    float _pct_five_hour = 23.0f;
    float _pct_seven_day = 47.0f;

    // Root container fills the screen and owns the click event.
    lv_obj_t* _root = nullptr;

    // Clock view widgets
    lv_obj_t* _clock_container = nullptr;
    lv_obj_t* _clock_canvas = nullptr;
    std::uint8_t* _clock_canvas_buf = nullptr;
    lv_obj_t* _clock_time_label = nullptr;
    lv_obj_t* _clock_date_label = nullptr;
    lv_obj_t* _clock_hint_label = nullptr;

    // Meter view widgets
    lv_obj_t* _meter_container = nullptr;
    lv_obj_t* _meter_title_label = nullptr;
    lv_obj_t* _meter_h5_label = nullptr;
    lv_obj_t* _meter_h5_pct_label = nullptr;
    lv_obj_t* _meter_h5_bar = nullptr;
    lv_obj_t* _meter_d7_label = nullptr;
    lv_obj_t* _meter_d7_pct_label = nullptr;
    lv_obj_t* _meter_d7_bar = nullptr;
    lv_obj_t* _meter_hint_label = nullptr;

    void _build_ui();
    void _build_clock_view();
    void _build_meter_view();
    void _toggle_view();
    void _show_view(View v);
    void _update_clock();
    void _update_meter();

    static void _on_root_clicked(lv_event_t* e);
};

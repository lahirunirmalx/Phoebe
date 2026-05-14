/**
 * @file app_claudemeter.h
 * @brief Claude API usage meter for the phoebe desktop sim.
 *
 * Mirrors the M5Cardputer-UserDemo app_claudemeter UI concept (5-hour and
 * 7-day usage bars, color-coded by threshold) and pulls real data from the
 * same /usage endpoint when HAL::ClaudeCfg() has a base URL + bearer token
 * configured. Falls back to drifting mock values otherwise so the UI is
 * still visible during development.
 *
 * The HTTP fetch runs on its own std::thread and writes results into a
 * mutex-guarded snapshot; the LVGL UI reads that snapshot in onRunning().
 */
#pragma once
#include <atomic>
#include <cstdint>
#include <mooncake.h>
#include <mutex>
#include <string>
#include <thread>
#include <lvgl.h>

class AppClaudeMeter : public mooncake::AppAbility {
public:
    AppClaudeMeter();
    ~AppClaudeMeter() override;

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    enum View { VIEW_CLOCK = 0, VIEW_METER = 1 };

    enum FetchState { Fetch_Idle = 0, Fetch_OK, Fetch_Err };

    View _view = VIEW_CLOCK;
    std::uint32_t _last_tick_ms = 0;

    // Background fetch -----------------------------------------------------
    std::thread _fetch_thread;
    std::atomic<bool> _fetch_stop{false};
    std::mutex _snapshot_mutex;

    struct Snapshot {
        float pct_five_hour = -1.0f;
        float pct_seven_day = -1.0f;
        FetchState state = Fetch_Idle;
        std::string last_err;
    } _snapshot;

    // Mock fallback when no Claude config is available.
    float _mock_pct_five_hour = 23.0f;
    float _mock_pct_seven_day = 47.0f;

    // Root container fills the screen and owns the click event.
    lv_obj_t* _root = nullptr;

    // Clock view widgets
    lv_obj_t* _clock_container = nullptr;
    lv_obj_t* _clock_canvas = nullptr;
    std::uint8_t* _clock_canvas_buf = nullptr;
    lv_obj_t* _clock_time_label = nullptr;
    lv_obj_t* _clock_date_label = nullptr;

    // Meter view widgets
    lv_obj_t* _meter_container = nullptr;
    lv_obj_t* _meter_title_label = nullptr;
    lv_obj_t* _meter_h5_label = nullptr;
    lv_obj_t* _meter_h5_pct_label = nullptr;
    lv_obj_t* _meter_h5_bar = nullptr;
    lv_obj_t* _meter_d7_label = nullptr;
    lv_obj_t* _meter_d7_pct_label = nullptr;
    lv_obj_t* _meter_d7_bar = nullptr;
    lv_obj_t* _meter_status_label = nullptr;

    void _build_ui();
    void _build_clock_view();
    void _build_meter_view();
    void _toggle_view();
    void _show_view(View v);
    void _update_clock();
    void _update_meter();

    void _start_fetch_thread();
    void _stop_fetch_thread();
    void _fetch_loop();
    bool _fetch_once(Snapshot& out);

    static void _on_root_clicked(lv_event_t* e);
};

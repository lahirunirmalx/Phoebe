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
#include <ctime>
#include <functional>
#include <mooncake.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
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
    // A cycleable screen: a full-screen container plus an update callback.
    // Tap cycles through _screens; double-tap pins the current one. Add a new
    // app by building its container and calling _register_screen() in _build_ui.
    struct Screen {
        std::string name;
        lv_obj_t* container = nullptr;
        std::function<void()> update;
    };

    enum FetchState { Fetch_Idle = 0, Fetch_OK, Fetch_Err };

    // Watch-face style. Resolved at onOpen() from SysCfg().watchFace.
    // ""/"analog" -> WF_Analog; "digital" -> WF_Digital; "animated" -> WF_Animated;
    // "seg7" -> WF_Seg7 (7-segment LCD); "vfd" -> WF_VFD (5x7 dot-matrix).
    enum WatchFace { WF_Analog = 0, WF_Digital, WF_Animated, WF_Seg7, WF_VFD };

    std::vector<Screen> _screens;           // registered screens, cycled by tap
    int _screen_idx = 0;                    // index of the currently shown screen
    WatchFace _watch_face = WF_Analog;
    std::uint32_t _last_tick_ms = 0;
    std::uint32_t _last_interaction_ms = 0; // for display-sleep timing
    std::uint32_t _last_click_ms = 0;       // for double-tap detection
    bool _pinned = false;                   // current screen pinned: no sleep, no cycle
    bool _display_on = true;                // our view of the backlight state

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

    // Boot splash shown until the clock is synced via SNTP.
    bool _booting = true;
    lv_obj_t* _boot_container = nullptr;
    lv_obj_t* _boot_arc = nullptr;
    lv_anim_t _boot_anim;

    // Clock view widgets (shared across faces)
    lv_obj_t* _clock_container = nullptr;
    lv_obj_t* _clock_time_label = nullptr;
    lv_obj_t* _clock_date_label = nullptr;
    lv_obj_t* _clock_5h_bar = nullptr;
    lv_obj_t* _clock_5h_pct_label = nullptr;

    // Analog-only: full-screen clock drawn with lightweight lv_line hands.
    lv_obj_t* _clock_canvas = nullptr;        // (unused by analog now; kept for cleanup)
    std::uint8_t* _clock_canvas_buf = nullptr;
    lv_obj_t* _hour_line = nullptr;
    lv_obj_t* _min_line = nullptr;
    lv_obj_t* _sec_line = nullptr;
    lv_point_precise_t _hour_pts[2];
    lv_point_precise_t _min_pts[2];
    lv_point_precise_t _sec_pts[2];

    // Digital-only
    lv_obj_t* _clock_sec_label = nullptr;

    // Animated-only
    lv_obj_t* _clock_anim_arc = nullptr;
    lv_anim_t _clock_anim;

    // Seg7 / VFD: canvas + backing buffer + last drawn second for cheap diffing.
    lv_obj_t* _clock_face_canvas = nullptr;
    std::uint8_t* _clock_face_canvas_buf = nullptr;
    int _clock_last_sec = -1;

    // Meter view widgets
    lv_obj_t* _meter_container = nullptr;
    lv_obj_t* _meter_title_label = nullptr;
    lv_obj_t* _meter_h5_label = nullptr;
    lv_obj_t* _meter_h5_pct_label = nullptr;
    lv_obj_t* _meter_h5_bar = nullptr;
    lv_obj_t* _meter_d7_label = nullptr;
    lv_obj_t* _meter_d7_pct_label = nullptr;
    lv_obj_t* _meter_d7_bar = nullptr;
    lv_obj_t* _meter_h5_arc = nullptr;   // round usage gauges
    lv_obj_t* _meter_d7_arc = nullptr;
    lv_obj_t* _meter_status_label = nullptr;

    // Weather view widgets + animated icon
    lv_obj_t* _weather_container = nullptr;
    lv_obj_t* _wx_city_label = nullptr;
    lv_obj_t* _wx_temp_label = nullptr;
    lv_obj_t* _wx_cond_label = nullptr;
    lv_obj_t* _wx_extra_label = nullptr;
    lv_obj_t* _wx_sun = nullptr;
    lv_obj_t* _wx_cloud = nullptr;
    lv_obj_t* _wx_drops[3] = {nullptr, nullptr, nullptr};

    // Background weather fetch (Open-Meteo)
    std::thread _weather_thread;
    std::atomic<bool> _weather_stop{false};
    std::mutex _weather_mutex;
    struct WeatherSnapshot {
        float temp_c = -1000.0f;
        float humidity = -1.0f;
        float wind_kmh = -1.0f;
        int code = -1;       // WMO weather code
        bool ok = false;
        std::string err;
    } _weather;
    int _wx_last_code = -2;  // for cheap icon-state diffing

    void _build_ui();
    void _build_clock_view();
    void _build_clock_5h_bar();
    void _build_clock_analog();
    void _build_clock_digital();
    void _build_clock_animated();
    void _build_clock_seg7();
    void _build_clock_vfd();
    void _build_meter_view();
    void _build_boot_screen();
    bool _time_is_synced() const;
    void _register_screen(const char* name, lv_obj_t* container, std::function<void()> update);
    void _show_screen(int idx);
    void _handle_tap();   // single = cycle screens, double = pin current, asleep = wake
    void _wake();         // turn backlight on, show first screen, unpin
    void _update_clock();
    void _update_clock_5h_bar();
    void _update_clock_analog(const struct tm& tm_info);
    void _update_clock_digital(const struct tm& tm_info);
    void _update_clock_animated(const struct tm& tm_info);
    void _update_clock_seg7(const struct tm& tm_info);
    void _update_clock_vfd(const struct tm& tm_info);
    void _update_meter();
    void _build_weather_view();
    void _update_weather();
    void _set_weather_icon(int code);     // show/hide icon parts for a WMO code
    void _start_weather_thread();
    void _stop_weather_thread();
    void _weather_loop();
    bool _weather_fetch_once(WeatherSnapshot& out);
    WatchFace _resolve_watch_face() const;

    void _start_fetch_thread();
    void _stop_fetch_thread();
    void _fetch_loop();
    bool _fetch_once(Snapshot& out);

    static void _on_root_clicked(lv_event_t* e);
};

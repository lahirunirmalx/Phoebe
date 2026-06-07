/**
 * @file app_claudemeter.cpp
 * @brief Claude usage meter app -- clock + meter views, click toggles.
 *        Real-time data: a background thread polls <base>/usage and updates
 *        a mutex-guarded snapshot. The UI reads the snapshot each tick.
 */
#include "app_claudemeter.h"
#include "hal/hal.h"
#include "weather_locations.h"
#include <ArduinoJson.h>
#include <cctype>
#include <chrono>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <mooncake_log.h>
#include <lvgl.h>

using namespace mooncake;

namespace {

// Screen geometry. Defaults to the production phoebe panel (144x168) but can be
// overridden per-platform via build flags -- e.g. the Freenove Mini TV passes
// -DPHOEBE_SCREEN_W=240 -DPHOEBE_SCREEN_H=240 so the faces fill its square panel.
#ifndef PHOEBE_SCREEN_W
#define PHOEBE_SCREEN_W 144
#endif
#ifndef PHOEBE_SCREEN_H
#define PHOEBE_SCREEN_H 168
#endif
constexpr int SCREEN_W = PHOEBE_SCREEN_W;
constexpr int SCREEN_H = PHOEBE_SCREEN_H;

// Turn the display (backlight) off after this long with no touch, unless the
// meter view is pinned. Only applies where the backlight is controllable.
constexpr std::uint32_t DISPLAY_SLEEP_MS = 5 * 60 * 1000; // 5 minutes
// Two taps within this window count as a double-tap (pin the meter).
constexpr std::uint32_t DOUBLE_TAP_MS = 400;

// Clock canvas geometry (square) -- sized to fill the 240x240 panel inside the
// usage rings on the analog face.
constexpr int CLOCK_CANVAS_W = 150;
constexpr int CLOCK_CANVAS_H = 150;

// Poll cadence for /usage. M5Cardputer-UserDemo uses 5 minutes; mirror it.
constexpr int FETCH_PERIOD_SEC = 300;
constexpr int FETCH_TIMEOUT_SEC = 8;

constexpr float WARN_THRESHOLD = 70.0f;
constexpr float DANGER_THRESHOLD = 90.0f;

const lv_color_t COLOR_BG = LV_COLOR_MAKE(0x00, 0x00, 0x00);
const lv_color_t COLOR_FG = LV_COLOR_MAKE(0xE6, 0xE6, 0xE6);
const lv_color_t COLOR_LABEL_DIM = LV_COLOR_MAKE(0x9A, 0x9A, 0x9A);
const lv_color_t COLOR_BAR_BG = LV_COLOR_MAKE(0x33, 0x33, 0x38);
const lv_color_t COLOR_OK = LV_COLOR_MAKE(0x99, 0xFF, 0x00);
const lv_color_t COLOR_WARN = LV_COLOR_MAKE(0xFF, 0xB0, 0x60);
const lv_color_t COLOR_DANGER = LV_COLOR_MAKE(0xFF, 0x64, 0x64);
const lv_color_t COLOR_ACCENT = LV_COLOR_MAKE(0x99, 0xFF, 0x00);

// Per-metric palettes so 5H and 7D are visually distinct on the rings + bars.
const lv_color_t COLOR_5H = LV_COLOR_MAKE(0x33, 0xC8, 0xFF); // cyan
const lv_color_t COLOR_7D = LV_COLOR_MAKE(0xFF, 0xC0, 0x40); // amber

// Weather icon palette.
const lv_color_t COLOR_SUN = LV_COLOR_MAKE(0xFF, 0xD2, 0x40);
const lv_color_t COLOR_CLOUD = LV_COLOR_MAKE(0xCA, 0xD2, 0xDE);
const lv_color_t COLOR_RAIN = LV_COLOR_MAKE(0x55, 0xAA, 0xFF);

// WMO weather code -> icon category and a short label.
enum WxCat { WX_CLEAR = 0, WX_CLOUD, WX_RAIN };
WxCat wx_category(int code)
{
    if (code <= 1) return WX_CLEAR;                 // 0 clear, 1 mainly clear
    if (code == 2 || code == 3 || code == 45 || code == 48) return WX_CLOUD;
    return WX_RAIN;                                 // drizzle/rain/snow/showers/thunder
}
const char* wx_text(int code)
{
    switch (code) {
        case 0:  return "Clear";
        case 1:  return "Mainly clear";
        case 2:  return "Partly cloudy";
        case 3:  return "Overcast";
        case 45:
        case 48: return "Fog";
        case 51:
        case 53:
        case 55: return "Drizzle";
        case 61:
        case 63:
        case 65: return "Rain";
        case 66:
        case 67: return "Freezing rain";
        case 71:
        case 73:
        case 75:
        case 77: return "Snow";
        case 80:
        case 81:
        case 82: return "Showers";
        case 85:
        case 86: return "Snow showers";
        case 95:
        case 96:
        case 99: return "Thunderstorm";
        default: return "--";
    }
}

// A metric keeps its own hue until usage hits the danger threshold, then turns
// red -- so 5H/7D stay visually distinct but high usage still reads as alarming.
lv_color_t metric_color(float pct, lv_color_t base)
{
    if (pct >= DANGER_THRESHOLD) return COLOR_DANGER;
    return base;
}

// Create a round gauge (270-degree arc, gap at the bottom, no knob, not
// interactive) used for the Claude usage rings on the meter + analog faces.
lv_obj_t* make_ring(lv_obj_t* parent, int size, int width)
{
    lv_obj_t* a = lv_arc_create(parent);
    lv_obj_set_size(a, size, size);
    lv_arc_set_rotation(a, 135);
    lv_arc_set_bg_angles(a, 0, 270);
    lv_arc_set_range(a, 0, 100);
    lv_arc_set_value(a, 0);
    lv_obj_remove_style(a, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(a, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, COLOR_BAR_BG, LV_PART_MAIN);
    return a;
}

void set_ring(lv_obj_t* a, float pct, lv_color_t color)
{
    if (!a) return;
    lv_arc_set_value(a, (int)(pct + 0.5f));
    lv_obj_set_style_arc_color(a, color, LV_PART_INDICATOR);
}

/* ---------------- 7-segment rendering ---------------- */

// Bit i set if segment i is on. Order: a b c d e f g.
//   aaa
//  f   b
//  f   b
//   ggg
//  e   c
//  e   c
//   ddd
constexpr std::uint8_t SEG7_DIGITS[10] = {
    0b0111111, // 0: a b c d e f
    0b0000110, // 1: b c
    0b1011011, // 2: a b d e g
    0b1001111, // 3: a b c d g
    0b1100110, // 4: b c f g
    0b1101101, // 5: a c d f g
    0b1111101, // 6: a c d e f g
    0b0000111, // 7: a b c
    0b1111111, // 8: all
    0b1101111, // 9: a b c d f g
};

const lv_color_t SEG7_ON = LV_COLOR_MAKE(0xFF, 0x30, 0x30);
const lv_color_t SEG7_OFF = LV_COLOR_MAKE(0x30, 0x05, 0x05);
const lv_color_t SEG7_BG = LV_COLOR_MAKE(0x0A, 0x00, 0x00);
const lv_color_t SEG7_BAR_BG = LV_COLOR_MAKE(0x1A, 0x00, 0x00);
const lv_color_t SEG7_DATE = LV_COLOR_MAKE(0xA0, 0x30, 0x30);

void draw_seg(lv_layer_t* layer, int x0, int y0, int x1, int y1, lv_color_t color)
{
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_color = color;
    r.bg_opa = LV_OPA_COVER;
    lv_area_t a = {x0, y0, x1 - 1, y1 - 1};
    lv_draw_rect(layer, &r, &a);
}

void draw_seg7_digit(lv_layer_t* layer, int x, int y, int digit, int dw, int dh, int t)
{
    std::uint8_t mask = (digit >= 0 && digit <= 9) ? SEG7_DIGITS[digit] : 0;
    int mid = dh / 2;
    auto seg = [&](int sx0, int sy0, int sx1, int sy1, int bit) {
        draw_seg(layer, x + sx0, y + sy0, x + sx1, y + sy1, (mask & (1 << bit)) ? SEG7_ON : SEG7_OFF);
    };
    seg(t, 0, dw - t, t, 0);                    // a
    seg(dw - t, t, dw, mid, 1);                 // b
    seg(dw - t, mid, dw, dh - t, 2);            // c
    seg(t, dh - t, dw - t, dh, 3);              // d
    seg(0, mid, t, dh - t, 4);                  // e
    seg(0, t, t, mid, 5);                       // f
    seg(t, mid - t / 2, dw - t, mid + t / 2, 6); // g
}

void draw_seg7_colon(lv_layer_t* layer, int x, int y, int w, int h, bool on)
{
    int dot = 4;
    lv_color_t c = on ? SEG7_ON : SEG7_OFF;
    int cx = x + (w - dot) / 2;
    int y1 = y + h / 3 - dot / 2;
    int y2 = y + 2 * h / 3 - dot / 2;
    draw_seg(layer, cx, y1, cx + dot, y1 + dot, c);
    draw_seg(layer, cx, y2, cx + dot, y2 + dot, c);
}

/* ---------------- VFD 5x7 dot-matrix font ----------------
 * Each glyph is 7 rows of 5 bits (MSB = leftmost pixel).
 * Index 0..9 == digits '0'..'9', index 10 == ':' (colon). */
constexpr std::uint8_t VFD_FONT[11][7] = {
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, // 2
    {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E}, // 3
    {0x11, 0x11, 0x11, 0x1F, 0x01, 0x01, 0x01}, // 4
    {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x11, 0x0E}, // 5
    {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}, // 9
    {0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00}, // :
};

const lv_color_t VFD_BG = LV_COLOR_MAKE(0x00, 0x08, 0x10);
const lv_color_t VFD_ON = LV_COLOR_MAKE(0x66, 0xFF, 0xCC);
const lv_color_t VFD_OFF = LV_COLOR_MAKE(0x0E, 0x18, 0x18);
const lv_color_t VFD_DIM = LV_COLOR_MAKE(0x3F, 0xAA, 0x88);

void draw_vfd_glyph(lv_layer_t* layer, int x, int y, int idx, int dot, int pitch)
{
    if (idx < 0 || idx > 10) return;
    lv_draw_rect_dsc_t r_on;
    lv_draw_rect_dsc_init(&r_on);
    r_on.bg_color = VFD_ON;
    r_on.bg_opa = LV_OPA_COVER;
    r_on.radius = LV_RADIUS_CIRCLE;

    lv_draw_rect_dsc_t r_off;
    lv_draw_rect_dsc_init(&r_off);
    r_off.bg_color = VFD_OFF;
    r_off.bg_opa = LV_OPA_COVER;
    r_off.radius = LV_RADIUS_CIRCLE;

    for (int row = 0; row < 7; row++) {
        std::uint8_t bits = VFD_FONT[idx][row];
        for (int col = 0; col < 5; col++) {
            int px = x + col * pitch;
            int py = y + row * pitch;
            lv_area_t a = {px, py, px + dot - 1, py + dot - 1};
            bool on = bits & (1 << (4 - col));
            lv_draw_rect(layer, on ? &r_on : &r_off, &a);
        }
    }
}

std::string trim_trailing_slash(const std::string& s)
{
    size_t end = s.size();
    while (end > 0 && s[end - 1] == '/') end--;
    return s.substr(0, end);
}

} // namespace

AppClaudeMeter::AppClaudeMeter()
{
    setAppInfo().name = "AppClaudeMeter";
}

AppClaudeMeter::~AppClaudeMeter()
{
    // Safety net: if onClose() didn't run (e.g. process killed mid-flight),
    // make sure the fetch thread isn't joinable when std::thread is destroyed.
    _fetch_stop.store(true);
    if (_fetch_thread.joinable()) _fetch_thread.join();
    _weather_stop.store(true);
    if (_weather_thread.joinable()) _weather_thread.join();
    _data_stop.store(true);
    if (_data_thread.joinable()) _data_thread.join();
}

void AppClaudeMeter::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
    open();
}

void AppClaudeMeter::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    lv_obj_set_scrollbar_mode(lv_screen_active(), LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(lv_screen_active(), COLOR_BG, 0);

    _watch_face = _resolve_watch_face();
    const char* wf_name = "analog";
    switch (_watch_face) {
        case WF_Digital:  wf_name = "digital";  break;
        case WF_Animated: wf_name = "animated"; break;
        case WF_Seg7:     wf_name = "seg7";     break;
        case WF_VFD:      wf_name = "vfd";      break;
        case WF_Analog:
        default:          wf_name = "analog";   break;
    }
    mclog::tagInfo(getAppInfo().name, "watch face: {}", wf_name);

    _build_ui();
    _update_meter();             // prime the meter widgets before they're shown

    // Show the boot splash; onRunning() reveals the clock once time syncs.
    for (auto& s : _screens) lv_obj_add_flag(s.container, LV_OBJ_FLAG_HIDDEN);
    _booting = true;

    // Boot lit; start the idle/sleep timer.
    _display_on = true;
    _pinned = false;
    _last_interaction_ms = HAL::SysCtrl().millis();
    _last_click_ms = 0;
    HAL::Backlight().on();

    // One unified network thread fetches Claude + weather + all extras serially
    // (see _data_loop) -- avoids concurrent TLS handshakes exhausting heap.
    _start_data_thread();
}

void AppClaudeMeter::onRunning()
{
    lv_timer_handler();

    const std::uint32_t now_ms = HAL::SysCtrl().millis();
    if (now_ms - _last_tick_ms < 500) return;
    _last_tick_ms = now_ms;

    // Stay on the boot splash until the clock is synced via SNTP.
    if (_booting) {
        if (_time_is_synced()) {
            _booting = false;
            _last_interaction_ms = now_ms;
            _show_screen(0); // reveal the clock
            mclog::tagInfo(getAppInfo().name, "time synced, showing clock");
        }
        return;
    }

    // Display sleep: turn the backlight off after the idle window, unless the
    // meter is pinned. Only on platforms with a controllable backlight.
    if (_display_on && !_pinned && HAL::Backlight().controllable() &&
        (now_ms - _last_interaction_ms > DISPLAY_SLEEP_MS)) {
        _display_on = false;
        HAL::Backlight().off();
    }

    // If we have no live data yet, drift the mock values so the rings/bars
    // remain visibly alive during development (affects whichever screen is up).
    {
        std::lock_guard<std::mutex> lock(_snapshot_mutex);
        if (_snapshot.state != Fetch_OK) {
            _mock_pct_five_hour += 0.7f;
            if (_mock_pct_five_hour > 100.0f) _mock_pct_five_hour = 0.0f;
            _mock_pct_seven_day += 0.3f;
            if (_mock_pct_seven_day > 100.0f) _mock_pct_seven_day = 0.0f;
        }
    }

    // Refresh the currently shown screen.
    if (_screen_idx < (int)_screens.size() && _screens[_screen_idx].update) {
        _screens[_screen_idx].update();
    }
}

void AppClaudeMeter::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _stop_fetch_thread();
    _stop_weather_thread();
    _stop_data_thread();

    if (_clock_anim_arc) {
        lv_anim_delete(_clock_anim_arc, NULL);
    }
    if (_boot_arc) {
        lv_anim_delete(_boot_arc, NULL);
    }
    if (_root) {
        lv_obj_delete(_root);
        _root = nullptr;
    }
    delete[] _clock_canvas_buf;
    _clock_canvas_buf = nullptr;
    delete[] _clock_face_canvas_buf;
    _clock_face_canvas_buf = nullptr;
}

void AppClaudeMeter::_build_ui()
{
    _root = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(_root);
    lv_obj_set_size(_root, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_root, 0, 0);
    lv_obj_set_style_bg_color(_root, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, 0);
    lv_obj_add_flag(_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_root, &AppClaudeMeter::_on_root_clicked, LV_EVENT_CLICKED, this);

    // Transparent full-screen tap-catcher on the top layer. The touch indev
    // always reports a press at screen-centre; without this, a centred clickable
    // decoration (e.g. the forecast icon, moon disc, pet face) would swallow the
    // tap and block screen cycling. The catcher sits above every screen's
    // widgets so a tap always reaches _handle_tap.
    lv_obj_t* tap = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(tap);
    lv_obj_set_size(tap, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(tap, 0, 0);
    lv_obj_set_style_bg_opa(tap, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(tap, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tap, &AppClaudeMeter::_on_root_clicked, LV_EVENT_CLICKED, this);

    // Boot splash (shown until the clock syncs via SNTP).
    _build_boot_screen();

    // Build each screen's container, then register it. Tap cycles in this order.
    _build_clock_view();
    _build_meter_view();
    _build_weather_view();
    _build_pomodoro_view();
    _build_world_view();
    _build_meeting_view();
    _build_currency_view();
    _build_aqi_view();
    _build_forecast_view();
    _build_sun_view();
    _build_net_view();
    _build_uptime_view();
    _build_pet_view();
    _build_saver_view();

    _register_screen("clock", _clock_container, [this] { _update_clock(); });
    _register_screen("meter", _meter_container, [this] { _update_meter(); });
    _register_screen("weather", _weather_container, [this] { _update_weather(); });
    _register_screen("pomodoro", _pomo_container, [this] { _update_pomodoro(); },
                     [this] { _pomodoro_on_show(); });
    _register_screen("world", _world_container, [this] { _update_world(); });
    _register_screen("meeting", _meet_container, [this] { _update_meeting(); });
    _register_screen("currency", _cur_container, [this] { _update_currency(); });
    _register_screen("aqi", _aqi_container, [this] { _update_aqi(); });
    _register_screen("forecast", _fc_container, [this] { _update_forecast(); });
    _register_screen("sunmoon", _sun_container, [this] { _update_sun(); });
    _register_screen("network", _net_container, [this] { _update_net(); });
    _register_screen("uptime", _up_container, [this] { _update_uptime(); });
    _register_screen("pet", _pet_container, [this] {});       // self-animating
    _register_screen("saver", _saver_container, [this] {});   // self-animating
}

void AppClaudeMeter::_build_boot_screen()
{
    _boot_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_boot_container);
    lv_obj_set_size(_boot_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_boot_container, 0, 0);
    lv_obj_set_style_bg_color(_boot_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_boot_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_boot_container, LV_OBJ_FLAG_CLICKABLE);

    // Spinning accent arc.
    _boot_arc = lv_arc_create(_boot_container);
    lv_obj_set_size(_boot_arc, 96, 96);
    lv_obj_align(_boot_arc, LV_ALIGN_CENTER, 0, -10);
    lv_obj_remove_style(_boot_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(_boot_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_bg_angles(_boot_arc, 0, 360);
    lv_arc_set_angles(_boot_arc, 0, 60);
    lv_obj_set_style_arc_color(_boot_arc, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_color(_boot_arc, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(_boot_arc, 7, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_boot_arc, 7, LV_PART_INDICATOR);

    lv_anim_init(&_boot_anim);
    lv_anim_set_var(&_boot_anim, _boot_arc);
    lv_anim_set_exec_cb(&_boot_anim, [](void* obj, int32_t v) {
        lv_arc_set_angles(static_cast<lv_obj_t*>(obj), v, v + 60);
    });
    lv_anim_set_values(&_boot_anim, 0, 360);
    lv_anim_set_duration(&_boot_anim, 1200);
    lv_anim_set_repeat_count(&_boot_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&_boot_anim);

    lv_obj_t* label = lv_label_create(_boot_container);
    lv_obj_set_style_text_color(label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_label_set_text(label, "syncing time...");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 60);
}

bool AppClaudeMeter::_time_is_synced() const
{
    time_t now = time(nullptr);
    return now > 1577836800; // > 2020-01-01 => SNTP has set the clock
}

void AppClaudeMeter::_register_screen(const char* name, lv_obj_t* container,
                                      std::function<void()> update,
                                      std::function<void()> on_show)
{
    _screens.push_back({name, container, std::move(update), std::move(on_show)});
}

void AppClaudeMeter::_show_screen(int idx)
{
    if (_screens.empty()) return;
    if (idx < 0) idx = 0;
    if (idx >= (int)_screens.size()) idx = (int)_screens.size() - 1;
    if (_boot_container) lv_obj_add_flag(_boot_container, LV_OBJ_FLAG_HIDDEN);
    _screen_idx = idx;
    for (int i = 0; i < (int)_screens.size(); ++i) {
        if (i == idx) {
            lv_obj_clear_flag(_screens[i].container, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(_screens[i].container, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (_screens[idx].on_show) _screens[idx].on_show();
    if (_screens[idx].update) _screens[idx].update();
    mclog::tagInfo(getAppInfo().name, "screen: {}", _screens[idx].name);
}

AppClaudeMeter::WatchFace AppClaudeMeter::_resolve_watch_face() const
{
    const auto& wf = HAL::SysCfg().getConfig().watchFace;
    if (wf == "digital") return WF_Digital;
    if (wf == "animated") return WF_Animated;
    if (wf == "seg7") return WF_Seg7;
    if (wf == "vfd") return WF_VFD;
    return WF_Analog;
}

void AppClaudeMeter::_build_clock_view()
{
    _clock_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_clock_container);
    lv_obj_set_size(_clock_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_clock_container, 0, 0);
    lv_obj_set_style_bg_color(_clock_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_clock_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_clock_container, LV_OBJ_FLAG_CLICKABLE);

    // Thin 5H usage bar across the top of every clock face.
    _build_clock_5h_bar();

    switch (_watch_face) {
        case WF_Digital:  _build_clock_digital();  break;
        case WF_Animated: _build_clock_animated(); break;
        case WF_Seg7:     _build_clock_seg7();     break;
        case WF_VFD:      _build_clock_vfd();      break;
        case WF_Analog:
        default:          _build_clock_analog();   break;
    }
}

void AppClaudeMeter::_build_clock_5h_bar()
{
    // Thin 5H Claude-usage bar at the very top, with a small pct label
    // on the right. Shared across all watch-face variants.
    _clock_5h_pct_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_5h_pct_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_clock_5h_pct_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_clock_5h_pct_label, "5H --");
    lv_obj_align(_clock_5h_pct_label, LV_ALIGN_TOP_RIGHT, -4, 0);

    _clock_5h_bar = lv_bar_create(_clock_container);
    lv_obj_set_size(_clock_5h_bar, SCREEN_W - 64, 4);
    lv_obj_align(_clock_5h_bar, LV_ALIGN_TOP_LEFT, 4, 8);
    lv_bar_set_range(_clock_5h_bar, 0, 100);
    lv_obj_set_style_bg_color(_clock_5h_bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_radius(_clock_5h_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_clock_5h_bar, 1, LV_PART_INDICATOR);
}

void AppClaudeMeter::_build_clock_analog()
{
    // Full-screen analog clock -- no Claude usage rings. Hands are lv_line
    // objects (cheap; no large canvas buffer), so they can fill the panel.
    // Nudged down a little so the dial clears the top 5H bar.
    const int OFF = 8;
    const int R = (SCREEN_W < SCREEN_H ? SCREEN_W : SCREEN_H) / 2 - 10;

    // Outer dial ring.
    lv_obj_t* dial = lv_arc_create(_clock_container);
    lv_obj_set_size(dial, R * 2, R * 2);
    lv_obj_align(dial, LV_ALIGN_CENTER, 0, OFF);
    lv_obj_remove_style(dial, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(dial, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_bg_angles(dial, 0, 360);
    lv_arc_set_angles(dial, 0, 0);
    lv_obj_set_style_arc_color(dial, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_width(dial, 3, LV_PART_MAIN);

    auto mk_hand = [&](lv_color_t color, int width) {
        lv_obj_t* l = lv_line_create(_clock_container);
        lv_obj_set_pos(l, 0, 0);
        lv_obj_set_style_line_color(l, color, 0);
        lv_obj_set_style_line_width(l, width, 0);
        lv_obj_set_style_line_rounded(l, true, 0);
        return l;
    };
    _hour_line = mk_hand(COLOR_FG, 8);
    _min_line = mk_hand(COLOR_FG, 5);
    _sec_line = mk_hand(COLOR_ACCENT, 3);

    // Center hub.
    lv_obj_t* hub = lv_obj_create(_clock_container);
    lv_obj_remove_style_all(hub);
    lv_obj_set_size(hub, 12, 12);
    lv_obj_set_style_radius(hub, 6, 0);
    lv_obj_set_style_bg_color(hub, COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
    lv_obj_align(hub, LV_ALIGN_CENTER, 0, OFF);

    // Date along the bottom edge.
    _clock_time_label = nullptr; // the hands are the time
    _clock_date_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_date_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_clock_date_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_clock_date_label, "");
    lv_obj_align(_clock_date_label, LV_ALIGN_BOTTOM_MID, 0, -6);
}

void AppClaudeMeter::_build_clock_digital()
{
    // Big HH:MM centered; smaller :SS below; date at the bottom.
    _clock_time_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_time_label, COLOR_FG, 0);
    lv_obj_set_style_text_font(_clock_time_label, &lv_font_montserrat_48, 0);
    lv_label_set_text(_clock_time_label, "00:00");
    lv_obj_align(_clock_time_label, LV_ALIGN_CENTER, 0, -20);

    _clock_sec_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_sec_label, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(_clock_sec_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(_clock_sec_label, ":00");
    lv_obj_align_to(_clock_sec_label, _clock_time_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

    _clock_date_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_date_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_clock_date_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_clock_date_label, "");
    lv_obj_align(_clock_date_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void AppClaudeMeter::_build_clock_animated()
{
    // Rotating accent arc behind the time. Driven by an lv_anim_t that
    // sweeps the arc start angle continuously, independent of wall time.
    _clock_anim_arc = lv_arc_create(_clock_container);
    lv_obj_set_size(_clock_anim_arc, 196, 196);
    lv_obj_align(_clock_anim_arc, LV_ALIGN_CENTER, 0, -4);
    lv_obj_remove_style(_clock_anim_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(_clock_anim_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_bg_angles(_clock_anim_arc, 0, 360);
    lv_arc_set_angles(_clock_anim_arc, 0, 60);
    lv_obj_set_style_arc_color(_clock_anim_arc, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_color(_clock_anim_arc, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(_clock_anim_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_clock_anim_arc, 10, LV_PART_INDICATOR);

    lv_anim_init(&_clock_anim);
    lv_anim_set_var(&_clock_anim, _clock_anim_arc);
    lv_anim_set_exec_cb(&_clock_anim, [](void* obj, int32_t v) {
        auto* arc = static_cast<lv_obj_t*>(obj);
        // Rotate a 60-degree sweep around the arc.
        lv_arc_set_angles(arc, v, v + 60);
    });
    lv_anim_set_values(&_clock_anim, 0, 360);
    lv_anim_set_duration(&_clock_anim, 2400);
    lv_anim_set_repeat_count(&_clock_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&_clock_anim);

    _clock_time_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_time_label, COLOR_FG, 0);
    lv_obj_set_style_text_font(_clock_time_label, &lv_font_montserrat_48, 0);
    lv_label_set_text(_clock_time_label, "00:00");
    lv_obj_align(_clock_time_label, LV_ALIGN_CENTER, 0, -4);

    _clock_date_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_date_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_clock_date_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_clock_date_label, "");
    lv_obj_align(_clock_date_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void AppClaudeMeter::_build_clock_seg7()
{
    // Full LCD theme: dark red body, red-tinted 5H bar, red date label.
    lv_obj_set_style_bg_color(_clock_container, SEG7_BG, 0);
    if (_clock_5h_bar) {
        lv_obj_set_style_bg_color(_clock_5h_bar, SEG7_BAR_BG, LV_PART_MAIN);
        lv_obj_set_style_bg_color(_clock_5h_bar, SEG7_ON, LV_PART_INDICATOR);
    }
    if (_clock_5h_pct_label) {
        lv_obj_set_style_text_color(_clock_5h_pct_label, SEG7_DATE, 0);
    }

    // Canvas hosts the 4 large 7-segment digits + a blinking colon.
    constexpr int CW = 212;
    constexpr int CH = 96;
    _clock_face_canvas_buf = new std::uint8_t[CW * CH * 2];
    _clock_face_canvas = lv_canvas_create(_clock_container);
    lv_canvas_set_buffer(_clock_face_canvas, _clock_face_canvas_buf, CW, CH,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_align(_clock_face_canvas, LV_ALIGN_CENTER, 0, -6);

    _clock_date_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_date_label, SEG7_DATE, 0);
    lv_obj_set_style_text_font(_clock_date_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_clock_date_label, "");
    lv_obj_align(_clock_date_label, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void AppClaudeMeter::_build_clock_vfd()
{
    // Full VFD theme: dark teal body, phosphor 5H bar, phosphor date label.
    lv_obj_set_style_bg_color(_clock_container, VFD_BG, 0);
    if (_clock_5h_bar) {
        lv_obj_set_style_bg_color(_clock_5h_bar, VFD_OFF, LV_PART_MAIN);
        lv_obj_set_style_bg_color(_clock_5h_bar, VFD_ON, LV_PART_INDICATOR);
    }
    if (_clock_5h_pct_label) {
        lv_obj_set_style_text_color(_clock_5h_pct_label, VFD_DIM, 0);
    }

    // Canvas hosts a 5x7 dot-matrix rendition of HH:MM in phosphor green.
    constexpr int CW = 224;
    constexpr int CH = 56;
    _clock_face_canvas_buf = new std::uint8_t[CW * CH * 2];
    _clock_face_canvas = lv_canvas_create(_clock_container);
    lv_canvas_set_buffer(_clock_face_canvas, _clock_face_canvas_buf, CW, CH,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_align(_clock_face_canvas, LV_ALIGN_CENTER, 0, -6);

    _clock_date_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_date_label, VFD_DIM, 0);
    lv_obj_set_style_text_font(_clock_date_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_clock_date_label, "");
    lv_obj_align(_clock_date_label, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void AppClaudeMeter::_build_meter_view()
{
    _meter_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_meter_container);
    lv_obj_set_size(_meter_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_meter_container, 0, 0);
    lv_obj_set_style_bg_color(_meter_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_meter_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_meter_container, LV_OBJ_FLAG_CLICKABLE);

    _meter_title_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_color(_meter_title_label, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(_meter_title_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_meter_title_label, "CLAUDE METER");
    lv_obj_align(_meter_title_label, LV_ALIGN_TOP_MID, 0, 12);

    // Concentric round gauges: outer = 7-day, inner = 5-hour. Sized to stay
    // well inside the panel (the case bezel crops ~15px at the edges) and to
    // clear the secondary bars at the bottom.
    const int outer = 150;
    _meter_d7_arc = make_ring(_meter_container, outer, 11);
    lv_obj_align(_meter_d7_arc, LV_ALIGN_CENTER, 0, -14);
    _meter_h5_arc = make_ring(_meter_container, outer - 42, 11);
    lv_obj_align(_meter_h5_arc, LV_ALIGN_CENTER, 0, -14);

    // Percentages in the centre of the rings.
    _meter_h5_pct_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_font(_meter_h5_pct_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(_meter_h5_pct_label, "5H --");
    lv_obj_align(_meter_h5_pct_label, LV_ALIGN_CENTER, 0, -24);

    _meter_d7_pct_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_font(_meter_d7_pct_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_meter_d7_pct_label, "7D --");
    lv_obj_align(_meter_d7_pct_label, LV_ALIGN_CENTER, 0, -2);

    // Thin secondary bars at the bottom (5H then 7D).
    _meter_h5_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_color(_meter_h5_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_meter_h5_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_meter_h5_label, "5H");
    lv_obj_align(_meter_h5_label, LV_ALIGN_BOTTOM_LEFT, 6, -44);

    _meter_h5_bar = lv_bar_create(_meter_container);
    lv_obj_set_size(_meter_h5_bar, SCREEN_W - 56, 8);
    lv_obj_align(_meter_h5_bar, LV_ALIGN_BOTTOM_LEFT, 40, -46);
    lv_bar_set_range(_meter_h5_bar, 0, 100);
    lv_obj_set_style_bg_color(_meter_h5_bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_radius(_meter_h5_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_meter_h5_bar, 2, LV_PART_INDICATOR);

    _meter_d7_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_color(_meter_d7_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_meter_d7_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_meter_d7_label, "7D");
    lv_obj_align(_meter_d7_label, LV_ALIGN_BOTTOM_LEFT, 6, -24);

    _meter_d7_bar = lv_bar_create(_meter_container);
    lv_obj_set_size(_meter_d7_bar, SCREEN_W - 56, 8);
    lv_obj_align(_meter_d7_bar, LV_ALIGN_BOTTOM_LEFT, 40, -26);
    lv_bar_set_range(_meter_d7_bar, 0, 100);
    lv_obj_set_style_bg_color(_meter_d7_bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_radius(_meter_d7_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_meter_d7_bar, 2, LV_PART_INDICATOR);

    _meter_status_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_color(_meter_status_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_meter_status_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_meter_status_label, "");
    lv_obj_align(_meter_status_label, LV_ALIGN_BOTTOM_MID, 0, -6);
}

void AppClaudeMeter::_update_clock()
{
    _update_clock_5h_bar();

    time_t now;
    struct tm* tm_info;
    time(&now);
    tm_info = localtime(&now);
    if (!tm_info) return;

    switch (_watch_face) {
        case WF_Digital:  _update_clock_digital(*tm_info);  break;
        case WF_Animated: _update_clock_animated(*tm_info); break;
        case WF_Seg7:     _update_clock_seg7(*tm_info);     break;
        case WF_VFD:      _update_clock_vfd(*tm_info);      break;
        case WF_Analog:
        default:          _update_clock_analog(*tm_info);   break;
    }
}

void AppClaudeMeter::_update_clock_5h_bar()
{
    if (!_clock_5h_bar) return;

    Snapshot snap;
    {
        std::lock_guard<std::mutex> lock(_snapshot_mutex);
        snap = _snapshot;
    }
    float p5 = (snap.state == Fetch_OK && snap.pct_five_hour >= 0.0f)
                   ? snap.pct_five_hour
                   : _mock_pct_five_hour;

    // For themed faces (seg7 / vfd) the bar colour is part of the face's
    // visual identity and must NOT change. Other faces use the 5H palette.
    bool themed = (_watch_face == WF_Seg7 || _watch_face == WF_VFD);
    lv_color_t c = themed ? lv_obj_get_style_bg_color(_clock_5h_bar, LV_PART_INDICATOR)
                          : metric_color(p5, COLOR_5H);

    lv_bar_set_value(_clock_5h_bar, (int)(p5 + 0.5f), LV_ANIM_OFF);
    if (!themed) {
        lv_obj_set_style_bg_color(_clock_5h_bar, c, LV_PART_INDICATOR);
    }

    char buf[12];
    std::snprintf(buf, sizeof(buf), "5H %d%%", (int)(p5 + 0.5f));
    lv_label_set_text(_clock_5h_pct_label, buf);
    if (!themed) {
        lv_obj_set_style_text_color(_clock_5h_pct_label, c, 0);
    }
}

void AppClaudeMeter::_update_clock_analog(const struct tm& tm_info)
{
    if (!_sec_line) return;

    char date_buf[40];
    std::snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d",
                  tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday);
    lv_label_set_text(_clock_date_label, date_buf);

    const float cx = SCREEN_W / 2.0f;
    const float cy = SCREEN_H / 2.0f + 8; // matches OFF in _build_clock_analog
    const int R = (SCREEN_W < SCREEN_H ? SCREEN_W : SCREEN_H) / 2 - 10;

    const int hour = tm_info.tm_hour % 12;
    const int minute = tm_info.tm_min;
    const int second = tm_info.tm_sec;

    const float ha = (hour + minute / 60.0f) * 30.0f * (float)M_PI / 180.0f;
    const float ma = (minute + second / 60.0f) * 6.0f * (float)M_PI / 180.0f;
    const float sa = second * 6.0f * (float)M_PI / 180.0f;

    // angle measured clockwise from 12 o'clock
    auto set_hand = [&](lv_obj_t* line, lv_point_precise_t* pts, float angle, float len) {
        pts[0].x = cx;
        pts[0].y = cy;
        pts[1].x = cx + len * std::sin(angle);
        pts[1].y = cy - len * std::cos(angle);
        lv_line_set_points(line, pts, 2);
    };

    set_hand(_hour_line, _hour_pts, ha, R * 0.50f);
    set_hand(_min_line, _min_pts, ma, R * 0.74f);
    set_hand(_sec_line, _sec_pts, sa, R * 0.90f);
}

void AppClaudeMeter::_update_clock_digital(const struct tm& tm_info)
{
    if (!_clock_time_label) return;

    char buf[40];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);
    lv_label_set_text(_clock_time_label, buf);

    if (_clock_sec_label) {
        std::snprintf(buf, sizeof(buf), ":%02d", tm_info.tm_sec);
        lv_label_set_text(_clock_sec_label, buf);
    }

    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                  tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday);
    lv_label_set_text(_clock_date_label, buf);
}

void AppClaudeMeter::_update_clock_animated(const struct tm& tm_info)
{
    if (!_clock_time_label) return;

    // The rotating arc keeps spinning on its own via lv_anim; we just
    // refresh the digital readout in the center.
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);
    lv_label_set_text(_clock_time_label, buf);

    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                  tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday);
    lv_label_set_text(_clock_date_label, buf);
}

void AppClaudeMeter::_update_clock_seg7(const struct tm& tm_info)
{
    if (!_clock_face_canvas) return;
    // Cheap diff: only redraw when the second has actually advanced --
    // the colon blinks per second, the digits change per minute.
    if (tm_info.tm_sec == _clock_last_sec) {
        // still update the date label outside the canvas
    } else {
        _clock_last_sec = tm_info.tm_sec;

        constexpr int CW = 212;
        constexpr int CH = 96;
        constexpr int DW = 36;
        constexpr int DH = 84;
        constexpr int T = 7;
        constexpr int GAP = 8;
        constexpr int COLON_W = 16;

        lv_canvas_fill_bg(_clock_face_canvas, SEG7_OFF, LV_OPA_TRANSP);
        // wipe the whole canvas to a darker bg first
        lv_draw_rect_dsc_t bg;
        lv_draw_rect_dsc_init(&bg);
        bg.bg_color = LV_COLOR_MAKE(0x0A, 0x00, 0x00);
        bg.bg_opa = LV_OPA_COVER;

        lv_layer_t layer;
        lv_canvas_init_layer(_clock_face_canvas, &layer);

        lv_area_t whole = {0, 0, CW - 1, CH - 1};
        lv_draw_rect(&layer, &bg, &whole);

        // total: 4 digits + colon + 4 gaps
        int total_w = 4 * DW + COLON_W + 4 * GAP;
        int x = (CW - total_w) / 2;
        int y = (CH - DH) / 2;

        int h1 = tm_info.tm_hour / 10;
        int h2 = tm_info.tm_hour % 10;
        int m1 = tm_info.tm_min / 10;
        int m2 = tm_info.tm_min % 10;

        draw_seg7_digit(&layer, x, y, h1, DW, DH, T);
        x += DW + GAP;
        draw_seg7_digit(&layer, x, y, h2, DW, DH, T);
        x += DW + GAP;
        draw_seg7_colon(&layer, x, y, COLON_W, DH, tm_info.tm_sec % 2 == 0);
        x += COLON_W + GAP;
        draw_seg7_digit(&layer, x, y, m1, DW, DH, T);
        x += DW + GAP;
        draw_seg7_digit(&layer, x, y, m2, DW, DH, T);

        lv_canvas_finish_layer(_clock_face_canvas, &layer);
    }

    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                  tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday);
    lv_label_set_text(_clock_date_label, buf);
}

void AppClaudeMeter::_update_clock_vfd(const struct tm& tm_info)
{
    if (!_clock_face_canvas) return;

    if (tm_info.tm_sec != _clock_last_sec) {
        _clock_last_sec = tm_info.tm_sec;

        constexpr int CW = 224;
        constexpr int CH = 56;
        constexpr int DOT = 5;
        constexpr int PITCH = 7;
        constexpr int GLYPH_W = 5 * PITCH; // 35
        constexpr int GLYPH_GAP = 7;
        // HH:MM = 5 glyphs separated by GLYPH_GAP
        constexpr int TOTAL_W = 5 * GLYPH_W + 4 * GLYPH_GAP;
        constexpr int TOTAL_H = 7 * PITCH;

        lv_draw_rect_dsc_t bg;
        lv_draw_rect_dsc_init(&bg);
        bg.bg_color = VFD_BG;
        bg.bg_opa = LV_OPA_COVER;

        lv_layer_t layer;
        lv_canvas_init_layer(_clock_face_canvas, &layer);

        lv_area_t whole = {0, 0, CW - 1, CH - 1};
        lv_draw_rect(&layer, &bg, &whole);

        int x = (CW - TOTAL_W) / 2;
        int y = (CH - TOTAL_H) / 2;

        int idxs[5] = {
            tm_info.tm_hour / 10,
            tm_info.tm_hour % 10,
            10, // colon
            tm_info.tm_min / 10,
            tm_info.tm_min % 10,
        };
        for (int i = 0; i < 5; i++) {
            draw_vfd_glyph(&layer, x, y, idxs[i], DOT, PITCH);
            x += GLYPH_W + GLYPH_GAP;
        }

        lv_canvas_finish_layer(_clock_face_canvas, &layer);
    }

    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                  tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday);
    lv_label_set_text(_clock_date_label, buf);
}

void AppClaudeMeter::_update_meter()
{
    if (!_meter_h5_bar) return;

    Snapshot snap;
    {
        std::lock_guard<std::mutex> lock(_snapshot_mutex);
        snap = _snapshot;
    }

    float p5;
    float p7;
    const char* status_text;
    if (snap.state == Fetch_OK && snap.pct_five_hour >= 0.0f && snap.pct_seven_day >= 0.0f) {
        p5 = snap.pct_five_hour;
        p7 = snap.pct_seven_day;
        status_text = "live";
    } else {
        p5 = _mock_pct_five_hour;
        p7 = _mock_pct_seven_day;
        status_text = snap.state == Fetch_Err ? snap.last_err.c_str() : "mock";
    }

    char buf[12];
    // 5H: cyan palette (red at high); the big centre number is just the pct.
    const lv_color_t h5c = metric_color(p5, COLOR_5H);
    std::snprintf(buf, sizeof(buf), "%d%%", (int)(p5 + 0.5f));
    lv_label_set_text(_meter_h5_pct_label, buf);
    lv_obj_set_style_text_color(_meter_h5_pct_label, h5c, 0);
    lv_bar_set_value(_meter_h5_bar, (int)(p5 + 0.5f), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_meter_h5_bar, h5c, LV_PART_INDICATOR);
    set_ring(_meter_h5_arc, p5, h5c);

    // 7D: amber palette (red at high); keeps its "7D xx%" label.
    const lv_color_t d7c = metric_color(p7, COLOR_7D);
    std::snprintf(buf, sizeof(buf), "7D %d%%", (int)(p7 + 0.5f));
    lv_label_set_text(_meter_d7_pct_label, buf);
    lv_obj_set_style_text_color(_meter_d7_pct_label, d7c, 0);
    lv_bar_set_value(_meter_d7_bar, (int)(p7 + 0.5f), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_meter_d7_bar, d7c, LV_PART_INDICATOR);
    set_ring(_meter_d7_arc, p7, d7c);

    lv_label_set_text(_meter_status_label, status_text);
}

/* ------------------------------ Weather -------------------------------- */

void AppClaudeMeter::_build_weather_view()
{
    _weather_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_weather_container);
    lv_obj_set_size(_weather_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_weather_container, 0, 0);
    lv_obj_set_style_bg_color(_weather_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_weather_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_weather_container, LV_OBJ_FLAG_CLICKABLE);

    _wx_city_label = lv_label_create(_weather_container);
    lv_obj_set_style_text_color(_wx_city_label, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(_wx_city_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_wx_city_label, "");
    lv_obj_align(_wx_city_label, LV_ALIGN_TOP_MID, 0, 8);

    // ---- Animated icon (centered around y ~= 78) -------------------------
    const int icy = 78;

    // Cloud: a rounded body + two puffs.
    _wx_cloud = lv_obj_create(_weather_container);
    lv_obj_remove_style_all(_wx_cloud);
    lv_obj_set_size(_wx_cloud, 78, 30);
    lv_obj_set_style_radius(_wx_cloud, 15, 0);
    lv_obj_set_style_bg_color(_wx_cloud, COLOR_CLOUD, 0);
    lv_obj_set_style_bg_opa(_wx_cloud, LV_OPA_COVER, 0);
    lv_obj_align(_wx_cloud, LV_ALIGN_TOP_MID, 0, icy);
    auto puff = [&](int dx, int sz) {
        lv_obj_t* p = lv_obj_create(_wx_cloud);
        lv_obj_remove_style_all(p);
        lv_obj_set_size(p, sz, sz);
        lv_obj_set_style_radius(p, sz / 2, 0);
        lv_obj_set_style_bg_color(p, COLOR_CLOUD, 0);
        lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
        lv_obj_align(p, LV_ALIGN_TOP_MID, dx, -sz / 2);
    };
    puff(-16, 26);
    puff(14, 32);

    // Sun (drawn after cloud so it sits in front when clear).
    _wx_sun = lv_obj_create(_weather_container);
    lv_obj_remove_style_all(_wx_sun);
    lv_obj_set_size(_wx_sun, 50, 50);
    lv_obj_set_style_radius(_wx_sun, 25, 0);
    lv_obj_set_style_bg_color(_wx_sun, COLOR_SUN, 0);
    lv_obj_set_style_bg_opa(_wx_sun, LV_OPA_COVER, 0);
    lv_obj_align(_wx_sun, LV_ALIGN_TOP_MID, 0, icy + 2);

    // Raindrops below the cloud.
    for (int i = 0; i < 3; ++i) {
        _wx_drops[i] = lv_obj_create(_weather_container);
        lv_obj_remove_style_all(_wx_drops[i]);
        lv_obj_set_size(_wx_drops[i], 4, 12);
        lv_obj_set_style_radius(_wx_drops[i], 2, 0);
        lv_obj_set_style_bg_color(_wx_drops[i], COLOR_RAIN, 0);
        lv_obj_set_style_bg_opa(_wx_drops[i], LV_OPA_COVER, 0);
        lv_obj_align(_wx_drops[i], LV_ALIGN_TOP_MID, (i - 1) * 18, icy + 36);
    }

    // ---- Stats -----------------------------------------------------------
    _wx_temp_label = lv_label_create(_weather_container);
    lv_obj_set_style_text_color(_wx_temp_label, COLOR_FG, 0);
    lv_obj_set_style_text_font(_wx_temp_label, &lv_font_montserrat_48, 0);
    lv_label_set_text(_wx_temp_label, "--");
    lv_obj_align(_wx_temp_label, LV_ALIGN_CENTER, 0, 36);

    _wx_cond_label = lv_label_create(_weather_container);
    lv_obj_set_style_text_color(_wx_cond_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_wx_cond_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_wx_cond_label, "");
    lv_obj_align(_wx_cond_label, LV_ALIGN_CENTER, 0, 74);

    _wx_extra_label = lv_label_create(_weather_container);
    lv_obj_set_style_text_color(_wx_extra_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_wx_extra_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_wx_extra_label, "");
    lv_obj_align(_wx_extra_label, LV_ALIGN_BOTTOM_MID, 0, -8);

    // ---- Animations ------------------------------------------------------
    // Sun "breathing" pulse (size + re-center each frame).
    static lv_anim_t sun_anim;
    lv_anim_init(&sun_anim);
    lv_anim_set_var(&sun_anim, _wx_sun);
    lv_anim_set_exec_cb(&sun_anim, [](void* obj, int32_t v) {
        auto* s = static_cast<lv_obj_t*>(obj);
        lv_obj_set_size(s, v, v);
        lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 78 + 2 + (50 - v) / 2);
    });
    lv_anim_set_values(&sun_anim, 46, 56);
    lv_anim_set_duration(&sun_anim, 1100);
    lv_anim_set_playback_duration(&sun_anim, 1100);
    lv_anim_set_repeat_count(&sun_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&sun_anim);

    // Falling raindrops (staggered).
    const int drop_top = icy + 30;
    for (int i = 0; i < 3; ++i) {
        static lv_anim_t drop_anim[3];
        lv_anim_init(&drop_anim[i]);
        lv_anim_set_var(&drop_anim[i], _wx_drops[i]);
        lv_anim_set_exec_cb(&drop_anim[i], [](void* obj, int32_t v) {
            lv_obj_set_y(static_cast<lv_obj_t*>(obj), v);
        });
        lv_anim_set_values(&drop_anim[i], drop_top, drop_top + 22);
        lv_anim_set_duration(&drop_anim[i], 650);
        lv_anim_set_delay(&drop_anim[i], i * 200);
        lv_anim_set_repeat_count(&drop_anim[i], LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&drop_anim[i]);
    }

    _set_weather_icon(-1); // hide all until first fetch
}

void AppClaudeMeter::_set_weather_icon(int code)
{
    if (!_wx_sun) return;
    const WxCat cat = (code < 0) ? WX_CLOUD : wx_category(code);
    auto show = [](lv_obj_t* o, bool v) {
        if (!o) return;
        if (v) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    };
    show(_wx_sun, cat == WX_CLEAR);
    show(_wx_cloud, cat != WX_CLEAR);
    for (int i = 0; i < 3; ++i) show(_wx_drops[i], cat == WX_RAIN);
}

void AppClaudeMeter::_update_weather()
{
    if (!_wx_temp_label) return;

    lv_label_set_text(_wx_city_label, HAL::SysCfg().getConfig().weatherCity.c_str());

    WeatherSnapshot w;
    {
        std::lock_guard<std::mutex> lock(_weather_mutex);
        w = _weather;
    }

    if (!w.ok) {
        lv_label_set_text(_wx_temp_label, "--");
        lv_label_set_text(_wx_cond_label, w.err.empty() ? "fetching..." : w.err.c_str());
        lv_label_set_text(_wx_extra_label, "");
        if (_wx_last_code != -1) { _set_weather_icon(-1); _wx_last_code = -1; }
        return;
    }

    char buf[24];
    std::snprintf(buf, sizeof(buf), "%d\xC2\xB0""C", (int)(w.temp_c + 0.5f)); // NN°C
    lv_label_set_text(_wx_temp_label, buf);
    lv_label_set_text(_wx_cond_label, wx_text(w.code));

    char extra[40];
    std::snprintf(extra, sizeof(extra), "%d%%  %d km/h",
                  (int)(w.humidity + 0.5f), (int)(w.wind_kmh + 0.5f));
    lv_label_set_text(_wx_extra_label, extra);

    if (w.code != _wx_last_code) {
        _set_weather_icon(w.code);
        _wx_last_code = w.code;
    }
}

void AppClaudeMeter::_start_weather_thread()
{
    if (_weather_thread.joinable()) return;
    _weather_stop.store(false);
    _weather_thread = std::thread([this] { _weather_loop(); });
}

void AppClaudeMeter::_stop_weather_thread()
{
    _weather_stop.store(true);
    if (_weather_thread.joinable()) _weather_thread.join();
}

void AppClaudeMeter::_weather_loop()
{
    while (!_weather_stop.load()) {
        WeatherSnapshot fresh;
        bool ok = _weather_fetch_once(fresh);
        {
            std::lock_guard<std::mutex> lock(_weather_mutex);
            if (ok) {
                _weather = fresh;
            } else {
                _weather.ok = false;
                _weather.err = fresh.err;
            }
        }
        if (ok) {
            mclog::tagInfo(getAppInfo().name, "weather ok: {:.0f}C code={}", fresh.temp_c, fresh.code);
        } else {
            mclog::tagWarn(getAppInfo().name, "weather err: {}", fresh.err);
        }
        // 15 min on success, retry every 20 s while erroring.
        int wait_sec = ok ? 900 : 20;
        for (int i = 0; i < wait_sec * 4 && !_weather_stop.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
}

bool AppClaudeMeter::_weather_fetch_once(WeatherSnapshot& out)
{
    const auto& loc = weather::find(HAL::SysCfg().getConfig().weatherCity);

    char url[200];
    std::snprintf(url, sizeof(url),
                  "https://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f"
                  "&current=temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code",
                  loc.lat, loc.lon);

    auto resp = HAL::Http().get(url, "", 15); // TLS handshake can be slow
    if (resp.http_code != 200) {
        out.err = resp.http_code == 0 ? (resp.error.empty() ? "net err" : resp.error)
                                      : "HTTP " + std::to_string(resp.http_code);
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, resp.body) != DeserializationError::Ok) {
        out.err = "bad json";
        return false;
    }
    JsonObject cur = doc["current"];
    if (cur.isNull()) {
        out.err = "no data";
        return false;
    }
    out.temp_c = cur["temperature_2m"] | -1000.0f;
    out.humidity = cur["relative_humidity_2m"] | -1.0f;
    out.wind_kmh = cur["wind_speed_10m"] | -1.0f;
    out.code = cur["weather_code"] | -1;
    out.ok = (out.temp_c > -100.0f);
    if (!out.ok) out.err = "no fields";
    return out.ok;
}

/* ------------------------------ Pomodoro ------------------------------- */

namespace {
constexpr int POMO_WORK_SEC = 25 * 60;
constexpr int POMO_BREAK_SEC = 5 * 60;
}

void AppClaudeMeter::_build_pomodoro_view()
{
    _pomo_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_pomo_container);
    lv_obj_set_size(_pomo_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_pomo_container, 0, 0);
    lv_obj_set_style_bg_color(_pomo_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_pomo_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_pomo_container, LV_OBJ_FLAG_CLICKABLE);

    _pomo_phase_label = lv_label_create(_pomo_container);
    lv_obj_set_style_text_font(_pomo_phase_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(_pomo_phase_label, "WORK");
    lv_obj_align(_pomo_phase_label, LV_ALIGN_TOP_MID, 0, 14);

    // Ring sized so its top clears the phase label and its inner gap is wider
    // than the 48px "MM:SS" text (no overlap). Centered low on the screen.
    _pomo_arc = make_ring(_pomo_container, 162, 10);
    lv_arc_set_bg_angles(_pomo_arc, 0, 360);   // full ring that drains
    lv_arc_set_rotation(_pomo_arc, 270);
    lv_obj_align(_pomo_arc, LV_ALIGN_CENTER, 0, 22);

    _pomo_time_label = lv_label_create(_pomo_container);
    lv_obj_set_style_text_color(_pomo_time_label, COLOR_FG, 0);
    lv_obj_set_style_text_font(_pomo_time_label, &lv_font_montserrat_48, 0);
    lv_label_set_text(_pomo_time_label, "25:00");
    lv_obj_align(_pomo_time_label, LV_ALIGN_CENTER, 0, 22);
}

void AppClaudeMeter::_pomodoro_on_show()
{
    // Restart a fresh work session whenever the user lands on this screen.
    _pomo_work = true;
    _pomo_phase_start_ms = HAL::SysCtrl().millis();
}

void AppClaudeMeter::_update_pomodoro()
{
    if (!_pomo_time_label) return;

    const std::uint32_t now = HAL::SysCtrl().millis();
    const int phase_len = _pomo_work ? POMO_WORK_SEC : POMO_BREAK_SEC;
    int elapsed = (int)((now - _pomo_phase_start_ms) / 1000);

    if (elapsed >= phase_len) {
        // Phase finished -> swap work/break and pulse the backlight.
        _pomo_work = !_pomo_work;
        _pomo_phase_start_ms = now;
        elapsed = 0;
        HAL::Backlight().notify(hal_components::BacklightBase::Notify_LimitReached);
    }

    const int remaining = phase_len - elapsed;
    const lv_color_t c = _pomo_work ? COLOR_7D : COLOR_OK; // amber work / green break
    lv_label_set_text(_pomo_phase_label, _pomo_work ? "WORK" : "BREAK");
    lv_obj_set_style_text_color(_pomo_phase_label, c, 0);

    char buf[24];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", remaining / 60, remaining % 60);
    lv_label_set_text(_pomo_time_label, buf);

    lv_arc_set_value(_pomo_arc, remaining * 100 / phase_len);
    lv_obj_set_style_arc_color(_pomo_arc, c, LV_PART_INDICATOR);
}

/* ----------------------------- World clock ----------------------------- */

void AppClaudeMeter::_build_world_view()
{
    _world_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_world_container);
    lv_obj_set_size(_world_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_world_container, 0, 0);
    lv_obj_set_style_bg_color(_world_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_world_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_world_container, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* title = lv_label_create(_world_container);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "WORLD CLOCK");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    for (int i = 0; i < 4; ++i) {
        _world_rows[i] = lv_label_create(_world_container);
        lv_obj_set_style_text_color(_world_rows[i], COLOR_FG, 0);
        lv_obj_set_style_text_font(_world_rows[i], &lv_font_montserrat_24, 0);
        lv_label_set_text(_world_rows[i], "");
        lv_obj_align(_world_rows[i], LV_ALIGN_TOP_MID, 0, 50 + i * 44);
    }
}

void AppClaudeMeter::_update_world()
{
    if (!_world_rows[0]) return;

    struct Zone { const char* name; int offsetMin; };
    const Zone zones[4] = {
        {"Local", HAL::SysCfg().getConfig().tzOffsetMin},
        {"London", 0},
        {"New York", -300},
        {"Tokyo", 540},
    };

    const time_t utc = time(nullptr);
    for (int i = 0; i < 4; ++i) {
        time_t t = utc + zones[i].offsetMin * 60;
        struct tm tmv;
        gmtime_r(&t, &tmv);
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%-9s %02d:%02d", zones[i].name, tmv.tm_hour, tmv.tm_min);
        lv_label_set_text(_world_rows[i], buf);
    }
}

/* --------------------- Next meeting / Currency / AQI ------------------- */

namespace {
// Portable struct-tm -> UTC epoch (newlib here has no timegm()).
long tm_to_utc_epoch(int year, int mon0, int mday, int hh, int mm, int ss)
{
    static const int cum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    long days = (long)(year - 1970) * 365 + (year - 1969) / 4 - (year - 1901) / 100 + (year - 1601) / 400;
    days += cum[mon0 % 12];
    if (mon0 > 1 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) days += 1;
    days += mday - 1;
    return ((days * 24 + hh) * 60 + mm) * 60 + ss;
}

// Parse an iCal DTSTART value like "20260607T093000Z" / "20260607" to a UTC epoch.
long parse_ics_dt(const std::string& s)
{
    if (s.size() < 8) return 0;
    for (int i = 0; i < 8; ++i) if (!isdigit((unsigned char)s[i])) return 0;
    int year = (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
    int mon0 = (s[4]-'0')*10 + (s[5]-'0') - 1;
    int mday = (s[6]-'0')*10 + (s[7]-'0');
    int hh = 0, mm = 0, ss = 0;
    if (s.size() >= 15 && s[8] == 'T') {
        hh = (s[9]-'0')*10 + (s[10]-'0');
        mm = (s[11]-'0')*10 + (s[12]-'0');
        ss = (s[13]-'0')*10 + (s[14]-'0');
    }
    return tm_to_utc_epoch(year, mon0, mday, hh, mm, ss); // TZID ignored -> UTC
}

lv_color_t aqi_color(int aqi)
{
    if (aqi < 0) return COLOR_LABEL_DIM;
    if (aqi <= 40) return COLOR_OK;
    if (aqi <= 80) return COLOR_WARN;
    return COLOR_DANGER;
}
const char* aqi_text(int aqi)
{
    if (aqi < 0) return "--";
    if (aqi <= 20) return "Good";
    if (aqi <= 40) return "Fair";
    if (aqi <= 60) return "Moderate";
    if (aqi <= 80) return "Poor";
    if (aqi <= 100) return "Very poor";
    return "Extreme";
}
} // namespace

void AppClaudeMeter::_build_meeting_view()
{
    _meet_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_meet_container);
    lv_obj_set_size(_meet_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_meet_container, 0, 0);
    lv_obj_set_style_bg_color(_meet_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_meet_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_meet_container, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* title = lv_label_create(_meet_container);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "NEXT MEETING");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    _meet_when_lbl = lv_label_create(_meet_container);
    lv_obj_set_style_text_color(_meet_when_lbl, COLOR_FG, 0);
    lv_obj_set_style_text_font(_meet_when_lbl, &lv_font_montserrat_48, 0);
    lv_label_set_text(_meet_when_lbl, "--");
    lv_obj_align(_meet_when_lbl, LV_ALIGN_CENTER, 0, -16);

    _meet_title_lbl = lv_label_create(_meet_container);
    lv_obj_set_style_text_color(_meet_title_lbl, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_meet_title_lbl, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(_meet_title_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(_meet_title_lbl, SCREEN_W - 24);
    lv_obj_set_style_text_align(_meet_title_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(_meet_title_lbl, "");
    lv_obj_align(_meet_title_lbl, LV_ALIGN_CENTER, 0, 44);
}

void AppClaudeMeter::_update_meeting()
{
    if (!_meet_when_lbl) return;
    MeetingSnap m;
    {
        std::lock_guard<std::mutex> lock(_data_mutex);
        m = _meet;
    }
    // No event found -> "Free"; a real error/hint -> show it.
    if (!m.ok) {
        if (m.err == "no upcoming" || m.err.empty()) {
            lv_label_set_text(_meet_when_lbl, "Free");
            lv_obj_set_style_text_color(_meet_when_lbl, COLOR_OK, 0);
            lv_label_set_text(_meet_title_lbl, "");
        } else {
            lv_label_set_text(_meet_when_lbl, "--");
            lv_obj_set_style_text_color(_meet_when_lbl, COLOR_FG, 0);
            lv_label_set_text(_meet_title_lbl, m.err.c_str());
        }
        return;
    }

    long secs = m.start_epoch - (long)time(nullptr);
    if (secs < 0) secs = 0;

    // Nothing within 2 hours -> show "Free".
    if (secs > 2 * 3600) {
        lv_label_set_text(_meet_when_lbl, "Free");
        lv_obj_set_style_text_color(_meet_when_lbl, COLOR_OK, 0);
        lv_label_set_text(_meet_title_lbl, "");
        return;
    }

    char when[24];
    if (secs <= 15 * 60) {
        // Imminent: live MM:SS countdown, highlighted.
        std::snprintf(when, sizeof(when), "%02ld:%02ld", secs / 60, secs % 60);
        lv_obj_set_style_text_color(_meet_when_lbl, COLOR_ACCENT, 0);
    } else {
        const long mins = secs / 60;
        if (mins < 60) std::snprintf(when, sizeof(when), "in %ldm", mins);
        else std::snprintf(when, sizeof(when), "in %ldh %ldm", mins / 60, mins % 60);
        lv_obj_set_style_text_color(_meet_when_lbl, COLOR_FG, 0);
    }
    lv_label_set_text(_meet_when_lbl, when);
    lv_label_set_text(_meet_title_lbl, m.title.c_str());
}

void AppClaudeMeter::_build_currency_view()
{
    _cur_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_cur_container);
    lv_obj_set_size(_cur_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_cur_container, 0, 0);
    lv_obj_set_style_bg_color(_cur_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_cur_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_cur_container, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* title = lv_label_create(_cur_container);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "FX -> LKR");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

    for (int i = 0; i < 3; ++i) {
        _cur_rows[i] = lv_label_create(_cur_container);
        lv_obj_set_style_text_color(_cur_rows[i], COLOR_FG, 0);
        lv_obj_set_style_text_font(_cur_rows[i], &lv_font_montserrat_24, 0);
        lv_label_set_text(_cur_rows[i], "");
        lv_obj_align(_cur_rows[i], LV_ALIGN_TOP_MID, 0, 60 + i * 50);
    }
}

void AppClaudeMeter::_update_currency()
{
    if (!_cur_rows[0]) return;
    CurrencySnap c;
    {
        std::lock_guard<std::mutex> lock(_data_mutex);
        c = _cur;
    }
    if (!c.ok) {
        lv_label_set_text(_cur_rows[0], c.err.empty() ? "fetching..." : c.err.c_str());
        lv_label_set_text(_cur_rows[1], "");
        lv_label_set_text(_cur_rows[2], "");
        return;
    }
    char b[24];
    std::snprintf(b, sizeof(b), "USD  %.1f", c.usd); lv_label_set_text(_cur_rows[0], b);
    std::snprintf(b, sizeof(b), "EUR  %.1f", c.eur); lv_label_set_text(_cur_rows[1], b);
    std::snprintf(b, sizeof(b), "GBP  %.1f", c.gbp); lv_label_set_text(_cur_rows[2], b);
}

void AppClaudeMeter::_build_aqi_view()
{
    _aqi_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_aqi_container);
    lv_obj_set_size(_aqi_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_aqi_container, 0, 0);
    lv_obj_set_style_bg_color(_aqi_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_aqi_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_aqi_container, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* title = lv_label_create(_aqi_container);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "AIR QUALITY");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    _aqi_arc = make_ring(_aqi_container, 150, 12);
    lv_arc_set_range(_aqi_arc, 0, 100);
    lv_obj_align(_aqi_arc, LV_ALIGN_CENTER, 0, -6);

    _aqi_value_lbl = lv_label_create(_aqi_container);
    lv_obj_set_style_text_color(_aqi_value_lbl, COLOR_FG, 0);
    lv_obj_set_style_text_font(_aqi_value_lbl, &lv_font_montserrat_48, 0);
    lv_label_set_text(_aqi_value_lbl, "--");
    lv_obj_align(_aqi_value_lbl, LV_ALIGN_CENTER, 0, -14);

    _aqi_sub_lbl = lv_label_create(_aqi_container);
    lv_obj_set_style_text_color(_aqi_sub_lbl, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_aqi_sub_lbl, &lv_font_montserrat_14, 0);
    lv_label_set_text(_aqi_sub_lbl, "");
    lv_obj_align(_aqi_sub_lbl, LV_ALIGN_CENTER, 0, 18);
}

void AppClaudeMeter::_update_aqi()
{
    if (!_aqi_value_lbl) return;
    AqiSnap a;
    {
        std::lock_guard<std::mutex> lock(_data_mutex);
        a = _aqi;
    }
    if (!a.ok) {
        lv_label_set_text(_aqi_value_lbl, "--");
        lv_label_set_text(_aqi_sub_lbl, a.err.empty() ? "fetching..." : a.err.c_str());
        return;
    }
    const lv_color_t c = aqi_color(a.aqi);
    char b[16];
    std::snprintf(b, sizeof(b), "%d", a.aqi);
    lv_label_set_text(_aqi_value_lbl, b);
    lv_obj_set_style_text_color(_aqi_value_lbl, c, 0);
    int v = a.aqi; if (v > 100) v = 100; if (v < 0) v = 0;
    lv_arc_set_value(_aqi_arc, v);
    lv_obj_set_style_arc_color(_aqi_arc, c, LV_PART_INDICATOR);
    char sub[40];
    std::snprintf(sub, sizeof(sub), "%s  PM2.5 %d", aqi_text(a.aqi), (int)(a.pm25 + 0.5f));
    lv_label_set_text(_aqi_sub_lbl, sub);
}

/* ---- shared extras fetch thread (meeting / currency / AQI) ------------- */

void AppClaudeMeter::_start_data_thread()
{
    if (_data_thread.joinable()) return;
    _data_stop.store(false);
    _data_thread = std::thread([this] { _data_loop(); });
}

void AppClaudeMeter::_stop_data_thread()
{
    _data_stop.store(true);
    if (_data_thread.joinable()) _data_thread.join();
}

void AppClaudeMeter::_data_loop()
{
    // SINGLE network thread for the whole app: Claude + weather + all extras are
    // fetched strictly one at a time here, so only ever one TLS connection is
    // open. Running them on separate threads caused concurrent TLS handshakes to
    // exhaust heap (-> "net err" + a frozen UI). Each source has its own cadence.
    using BL = hal_components::BacklightBase;
    std::uint32_t last_claude = 0, last_weather = 0, last_daily = 0, last_aqi = 0;
    std::uint32_t last_cur = 0, last_net = 0, last_up = 0, last_meet = 0;
    bool first = true;
    bool prev_err = false, prev_limit = false;

    while (!_data_stop.load()) {
        const std::uint32_t now = HAL::SysCtrl().millis() / 1000;
        auto due = [&](std::uint32_t& last, std::uint32_t period) {
            if (first || now - last >= period) { last = now; return true; }
            return false;
        };
        // On a failed fetch, retry in ~20s instead of waiting the full period
        // (important at boot, when the first pass runs before WiFi associates).
        auto retry = [&](std::uint32_t& last, std::uint32_t period, bool ok) {
            if (!ok && period > 25) last = now - period + 20;
        };

        // --- Claude usage (drives the meter + backlight notifications) -----
        if (due(last_claude, 180)) {
            Snapshot f;
            bool ok = _fetch_once(f);
            {
                std::lock_guard<std::mutex> lock(_snapshot_mutex);
                if (ok) _snapshot = f;
                else { _snapshot.state = Fetch_Err; _snapshot.last_err = f.last_err; }
            }
            if (ok) {
                const bool limit = (f.pct_five_hour >= DANGER_THRESHOLD) ||
                                   (f.pct_seven_day >= DANGER_THRESHOLD);
                HAL::Backlight().notify((limit && !prev_limit) ? BL::Notify_LimitReached : BL::Notify_FetchOk);
                prev_limit = limit; prev_err = false;
            } else {
                if (!prev_err) HAL::Backlight().notify(BL::Notify_FetchErr);
                prev_err = true; prev_limit = false;
            }
            retry(last_claude, 180, ok);
        }

        if (due(last_weather, 600)) {
            WeatherSnapshot w; bool ok = _weather_fetch_once(w);
            { std::lock_guard<std::mutex> lock(_weather_mutex);
              if (ok) _weather = w; else { _weather.ok = false; _weather.err = w.err; } }
            retry(last_weather, 600, ok);
        }
        if (due(last_daily, 600)) {
            ForecastSnap fc; SunSnap sun; bool ok = _fetch_daily(fc, sun);
            { std::lock_guard<std::mutex> lock(_data_mutex);
              if (ok) { _fc = fc; _sun = sun; }
              else { _fc.ok = false; _fc.err = fc.err; _sun.ok = false; _sun.err = sun.err; } }
            mclog::tagInfo(getAppInfo().name, "daily {}", ok ? "ok" : fc.err);
            retry(last_daily, 600, ok);
        }
        if (due(last_aqi, 900)) {
            AqiSnap a; bool ok = _fetch_aqi(a);
            { std::lock_guard<std::mutex> lock(_data_mutex);
              if (ok) _aqi = a; else { _aqi.ok = false; _aqi.err = a.err; } }
            mclog::tagInfo(getAppInfo().name, "aqi {}", ok ? "ok" : a.err);
            retry(last_aqi, 900, ok);
        }
        if (due(last_cur, 1800)) {
            CurrencySnap c; bool ok = _fetch_currency(c);
            { std::lock_guard<std::mutex> lock(_data_mutex);
              if (ok) _cur = c; else { _cur.ok = false; _cur.err = c.err; } }
            mclog::tagInfo(getAppInfo().name, "currency {}", ok ? "ok" : c.err);
            retry(last_cur, 1800, ok);
        }
        if (due(last_meet, 300)) {
            MeetingSnap m; bool ok = _fetch_meeting(m);
            { std::lock_guard<std::mutex> lock(_data_mutex);
              if (ok) _meet = m; else { _meet.ok = false; _meet.err = m.err; } }
            retry(last_meet, 300, ok);
        }
        if (due(last_net, 90)) {
            NetSnap n; bool ok = _fetch_net(n);
            { std::lock_guard<std::mutex> lock(_data_mutex);
              if (ok) _net = n; else { _net.ok = false; _net.err = n.err; } }
            retry(last_net, 90, ok);
        }
        if (due(last_up, 120)) {
            UpSnap u; bool ok = _fetch_uptime(u);
            { std::lock_guard<std::mutex> lock(_data_mutex);
              if (ok) _up = u; else { _up.ok = false; _up.err = u.err; } }
            retry(last_up, 120, ok);
        }

        first = false;
        for (int i = 0; i < 4 && !_data_stop.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
}

bool AppClaudeMeter::_fetch_meeting(MeetingSnap& out)
{
    const std::string url = HAL::SysCfg().getConfig().icsUrl;
    if (url.empty()) { out.err = "set .ics URL"; return false; }

    auto resp = HAL::Http().get(url, "", 15);
    if (resp.http_code != 200) {
        out.err = resp.http_code == 0 ? "net err" : "HTTP " + std::to_string(resp.http_code);
        return false;
    }

    const long now = (long)time(nullptr);
    long best = 0;
    std::string best_title, cur_summary, cur_dt;
    bool in_event = false;
    const std::string& b = resp.body;
    size_t pos = 0;
    while (pos < b.size()) {
        size_t eol = b.find('\n', pos);
        if (eol == std::string::npos) eol = b.size();
        std::string line = b.substr(pos, eol - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        pos = eol + 1;

        if (line.rfind("BEGIN:VEVENT", 0) == 0) { in_event = true; cur_summary.clear(); cur_dt.clear(); }
        else if (line.rfind("END:VEVENT", 0) == 0) {
            long st = parse_ics_dt(cur_dt);
            if (st >= now && (best == 0 || st < best)) { best = st; best_title = cur_summary; }
            in_event = false;
        } else if (in_event) {
            if (line.rfind("SUMMARY", 0) == 0) {
                size_t c = line.find(':');
                if (c != std::string::npos) cur_summary = line.substr(c + 1);
            } else if (line.rfind("DTSTART", 0) == 0) {
                size_t c = line.find(':');
                if (c != std::string::npos) cur_dt = line.substr(c + 1);
            }
        }
    }
    if (best == 0) { out.err = "no upcoming"; return false; }
    out.start_epoch = best;
    out.title = best_title.empty() ? "(no title)" : best_title;
    out.ok = true;
    return true;
}

bool AppClaudeMeter::_fetch_currency(CurrencySnap& out)
{
    auto resp = HAL::Http().get("https://open.er-api.com/v6/latest/USD", "", 12);
    if (resp.http_code != 200) {
        out.err = resp.http_code == 0 ? "net err" : "HTTP " + std::to_string(resp.http_code);
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, resp.body) != DeserializationError::Ok) { out.err = "bad json"; return false; }
    float lkr = doc["rates"]["LKR"] | -1.0f;
    float eur = doc["rates"]["EUR"] | -1.0f;
    float gbp = doc["rates"]["GBP"] | -1.0f;
    if (lkr <= 0) { out.err = "no rates"; return false; }
    out.usd = lkr;                          // 1 USD -> LKR
    out.eur = (eur > 0) ? lkr / eur : -1;   // 1 EUR -> LKR
    out.gbp = (gbp > 0) ? lkr / gbp : -1;   // 1 GBP -> LKR
    out.ok = true;
    return true;
}

bool AppClaudeMeter::_fetch_aqi(AqiSnap& out)
{
    const auto& loc = weather::find(HAL::SysCfg().getConfig().weatherCity);
    char url[200];
    std::snprintf(url, sizeof(url),
                  "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.3f&longitude=%.3f"
                  "&current=european_aqi,pm2_5",
                  loc.lat, loc.lon);
    auto resp = HAL::Http().get(url, "", 12);
    if (resp.http_code != 200) {
        out.err = resp.http_code == 0 ? "net err" : "HTTP " + std::to_string(resp.http_code);
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, resp.body) != DeserializationError::Ok) { out.err = "bad json"; return false; }
    JsonObject cur = doc["current"];
    if (cur.isNull()) { out.err = "no data"; return false; }
    out.aqi = cur["european_aqi"] | -1;
    out.pm25 = cur["pm2_5"] | -1.0f;
    out.ok = (out.aqi >= 0);
    if (!out.ok) out.err = "no fields";
    return out.ok;
}

/* ---------------- Forecast / Sun-moon / Network ------------------------ */

namespace {
const char* WDAY[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// Style a small forecast icon obj for a weather category.
void style_mini_icon(lv_obj_t* o, int code)
{
    if (!o) return;
    lv_color_t col = COLOR_CLOUD;
    int radius = 6;
    switch (wx_category(code)) {
        case WX_CLEAR: col = COLOR_SUN; radius = 14; break;  // round = sun
        case WX_CLOUD: col = COLOR_CLOUD; radius = 6; break;
        case WX_RAIN:  col = COLOR_RAIN; radius = 6; break;
    }
    lv_obj_set_style_bg_color(o, col, 0);
    lv_obj_set_style_radius(o, radius, 0);
}
} // namespace

void AppClaudeMeter::_build_forecast_view()
{
    _fc_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_fc_container);
    lv_obj_set_size(_fc_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_fc_container, 0, 0);
    lv_obj_set_style_bg_color(_fc_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_fc_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_fc_container, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* title = lv_label_create(_fc_container);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "3-DAY FORECAST");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    for (int i = 0; i < 3; ++i) {
        const int dx = (i - 1) * 74;
        _fc_name[i] = lv_label_create(_fc_container);
        lv_obj_set_style_text_color(_fc_name[i], COLOR_FG, 0);
        lv_obj_set_style_text_font(_fc_name[i], &lv_font_montserrat_24, 0);
        lv_label_set_text(_fc_name[i], "");
        lv_obj_align(_fc_name[i], LV_ALIGN_TOP_MID, dx, 52);

        _fc_icon[i] = lv_obj_create(_fc_container);
        lv_obj_remove_style_all(_fc_icon[i]);
        lv_obj_set_size(_fc_icon[i], 30, 30);
        lv_obj_set_style_bg_opa(_fc_icon[i], LV_OPA_COVER, 0);
        lv_obj_align(_fc_icon[i], LV_ALIGN_TOP_MID, dx, 96);

        _fc_temp[i] = lv_label_create(_fc_container);
        lv_obj_set_style_text_color(_fc_temp[i], COLOR_LABEL_DIM, 0);
        lv_obj_set_style_text_font(_fc_temp[i], &lv_font_montserrat_14, 0);
        lv_label_set_text(_fc_temp[i], "");
        lv_obj_align(_fc_temp[i], LV_ALIGN_TOP_MID, dx, 146);
    }
}

void AppClaudeMeter::_update_forecast()
{
    if (!_fc_name[0]) return;
    ForecastSnap fc;
    {
        std::lock_guard<std::mutex> lock(_data_mutex);
        fc = _fc;
    }
    for (int i = 0; i < 3; ++i) {
        if (!fc.ok) {
            lv_label_set_text(_fc_name[i], i == 0 ? (fc.err.empty() ? "..." : fc.err.c_str()) : "");
            lv_label_set_text(_fc_temp[i], "");
            lv_obj_add_flag(_fc_icon[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(_fc_icon[i], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(_fc_name[i], i == 0 ? "Today" : WDAY[fc.d[i].wday % 7]);
        style_mini_icon(_fc_icon[i], fc.d[i].code);
        char b[24];
        std::snprintf(b, sizeof(b), "%d/%d", (int)(fc.d[i].tmax + 0.5f), (int)(fc.d[i].tmin + 0.5f));
        lv_label_set_text(_fc_temp[i], b);
    }
}

void AppClaudeMeter::_build_sun_view()
{
    _sun_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_sun_container);
    lv_obj_set_size(_sun_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_sun_container, 0, 0);
    lv_obj_set_style_bg_color(_sun_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_sun_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_sun_container, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* title = lv_label_create(_sun_container);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "SUN & MOON");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    _sun_rise_lbl = lv_label_create(_sun_container);
    lv_obj_set_style_text_color(_sun_rise_lbl, COLOR_SUN, 0);
    lv_obj_set_style_text_font(_sun_rise_lbl, &lv_font_montserrat_24, 0);
    lv_label_set_text(_sun_rise_lbl, "rise --:--");
    lv_obj_align(_sun_rise_lbl, LV_ALIGN_TOP_MID, 0, 42);

    _sun_set_lbl = lv_label_create(_sun_container);
    lv_obj_set_style_text_color(_sun_set_lbl, COLOR_7D, 0);
    lv_obj_set_style_text_font(_sun_set_lbl, &lv_font_montserrat_24, 0);
    lv_label_set_text(_sun_set_lbl, "set --:--");
    lv_obj_align(_sun_set_lbl, LV_ALIGN_TOP_MID, 0, 76);

    // Moon: white disc + offset bg-shadow circle approximates the phase.
    _moon_disc = lv_obj_create(_sun_container);
    lv_obj_remove_style_all(_moon_disc);
    lv_obj_set_size(_moon_disc, 70, 70);
    lv_obj_set_style_radius(_moon_disc, 35, 0);
    lv_obj_set_style_bg_color(_moon_disc, lv_color_hex(0xEFEFEF), 0);
    lv_obj_set_style_bg_opa(_moon_disc, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(_moon_disc, true, 0);
    lv_obj_align(_moon_disc, LV_ALIGN_CENTER, 0, 28);

    _moon_shadow = lv_obj_create(_moon_disc);
    lv_obj_remove_style_all(_moon_shadow);
    lv_obj_set_size(_moon_shadow, 70, 70);
    lv_obj_set_style_radius(_moon_shadow, 35, 0);
    lv_obj_set_style_bg_color(_moon_shadow, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_moon_shadow, LV_OPA_COVER, 0);
    lv_obj_align(_moon_shadow, LV_ALIGN_CENTER, 0, 0);

    _moon_name_lbl = lv_label_create(_sun_container);
    lv_obj_set_style_text_color(_moon_name_lbl, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_moon_name_lbl, &lv_font_montserrat_14, 0);
    lv_label_set_text(_moon_name_lbl, "");
    lv_obj_align(_moon_name_lbl, LV_ALIGN_BOTTOM_MID, 0, -8);
}

void AppClaudeMeter::_update_sun()
{
    if (!_sun_rise_lbl) return;
    SunSnap s;
    {
        std::lock_guard<std::mutex> lock(_data_mutex);
        s = _sun;
    }
    char b[24];
    std::snprintf(b, sizeof(b), "rise %s", s.ok ? s.rise : "--:--"); lv_label_set_text(_sun_rise_lbl, b);
    std::snprintf(b, sizeof(b), "set  %s", s.ok ? s.set : "--:--"); lv_label_set_text(_sun_set_lbl, b);

    // Moon phase from a known new moon (2000-01-06 18:14 UTC).
    const double SYNODIC = 29.530588853;
    const long ref = 947182440;
    double age = (double)((long)time(nullptr) - ref) / 86400.0;
    age = age - SYNODIC * std::floor(age / SYNODIC); // 0..29.53
    const double illum = (1.0 - std::cos(2.0 * M_PI * age / SYNODIC)) / 2.0;
    const bool waxing = age < SYNODIC / 2.0;

    // Offset the shadow circle: 0 illum -> centered (dark/new); full -> off-disc.
    const int dx = (int)((waxing ? -1 : 1) * illum * 140.0); // 140 = 2*diameter-ish
    lv_obj_align(_moon_shadow, LV_ALIGN_CENTER, dx, 0);

    const char* name = "New Moon";
    if (age < 1.8) name = "New Moon";
    else if (age < 5.5) name = "Waxing Crescent";
    else if (age < 9.2) name = "First Quarter";
    else if (age < 12.9) name = "Waxing Gibbous";
    else if (age < 16.6) name = "Full Moon";
    else if (age < 20.3) name = "Waning Gibbous";
    else if (age < 23.9) name = "Last Quarter";
    else if (age < 27.6) name = "Waning Crescent";
    char mb[40];
    std::snprintf(mb, sizeof(mb), "%s  %d%%", name, (int)(illum * 100 + 0.5));
    lv_label_set_text(_moon_name_lbl, mb);
}

void AppClaudeMeter::_build_net_view()
{
    _net_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_net_container);
    lv_obj_set_size(_net_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_net_container, 0, 0);
    lv_obj_set_style_bg_color(_net_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_net_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_net_container, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* title = lv_label_create(_net_container);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "NETWORK PING");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    _net_arc = make_ring(_net_container, 150, 12);
    lv_arc_set_range(_net_arc, 0, 100);
    lv_obj_align(_net_arc, LV_ALIGN_CENTER, 0, -6);

    _net_value_lbl = lv_label_create(_net_container);
    lv_obj_set_style_text_color(_net_value_lbl, COLOR_FG, 0);
    lv_obj_set_style_text_font(_net_value_lbl, &lv_font_montserrat_48, 0);
    lv_label_set_text(_net_value_lbl, "--");
    lv_obj_align(_net_value_lbl, LV_ALIGN_CENTER, 0, -14);

    _net_sub_lbl = lv_label_create(_net_container);
    lv_obj_set_style_text_color(_net_sub_lbl, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_net_sub_lbl, &lv_font_montserrat_14, 0);
    lv_label_set_text(_net_sub_lbl, "");
    lv_obj_align(_net_sub_lbl, LV_ALIGN_CENTER, 0, 20);
}

void AppClaudeMeter::_update_net()
{
    if (!_net_value_lbl) return;
    NetSnap n;
    {
        std::lock_guard<std::mutex> lock(_data_mutex);
        n = _net;
    }
    if (!n.ok) {
        lv_label_set_text(_net_value_lbl, "--");
        lv_label_set_text(_net_sub_lbl, n.err.empty() ? "measuring..." : n.err.c_str());
        return;
    }
    const lv_color_t c = (n.latency_ms < 100) ? COLOR_OK : (n.latency_ms < 300 ? COLOR_WARN : COLOR_DANGER);
    char b[16];
    std::snprintf(b, sizeof(b), "%d", n.latency_ms);
    lv_label_set_text(_net_value_lbl, b);
    lv_obj_set_style_text_color(_net_value_lbl, c, 0);
    int v = 100 - n.latency_ms / 5; if (v < 0) v = 0; if (v > 100) v = 100;
    lv_arc_set_value(_net_arc, v);
    lv_obj_set_style_arc_color(_net_arc, c, LV_PART_INDICATOR);
    lv_label_set_text(_net_sub_lbl,
                      n.latency_ms < 100 ? "ms  good" : (n.latency_ms < 300 ? "ms  ok" : "ms  poor"));
}

bool AppClaudeMeter::_fetch_daily(ForecastSnap& fc, SunSnap& sun)
{
    const auto& loc = weather::find(HAL::SysCfg().getConfig().weatherCity);
    char url[256];
    std::snprintf(url, sizeof(url),
                  "https://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f"
                  "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset"
                  "&forecast_days=3&timezone=auto",
                  loc.lat, loc.lon);
    auto resp = HAL::Http().get(url, "", 15);
    if (resp.http_code != 200) {
        fc.err = sun.err = (resp.http_code == 0 ? "net err" : "HTTP " + std::to_string(resp.http_code));
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, resp.body) != DeserializationError::Ok) { fc.err = sun.err = "bad json"; return false; }
    JsonObject d = doc["daily"];
    if (d.isNull()) { fc.err = sun.err = "no data"; return false; }
    for (int i = 0; i < 3; ++i) {
        fc.d[i].code = d["weather_code"][i] | -1;
        fc.d[i].tmax = d["temperature_2m_max"][i] | 0.0f;
        fc.d[i].tmin = d["temperature_2m_min"][i] | 0.0f;
        const char* date = d["time"][i] | "";
        if (strlen(date) >= 10) {
            int y = (date[0]-'0')*1000 + (date[1]-'0')*100 + (date[2]-'0')*10 + (date[3]-'0');
            int mo = (date[5]-'0')*10 + (date[6]-'0') - 1;
            int da = (date[8]-'0')*10 + (date[9]-'0');
            long days = tm_to_utc_epoch(y, mo, da, 0, 0, 0) / 86400;
            fc.d[i].wday = (int)(((days % 7) + 4 + 7) % 7); // 1970-01-01 = Thursday(4)
        }
    }
    fc.ok = true;
    // sunrise/sunset are local ISO "YYYY-MM-DDTHH:MM"; take HH:MM (chars 11..15).
    const char* sr = d["sunrise"][0] | "";
    const char* ssr = d["sunset"][0] | "";
    if (strlen(sr) >= 16) { memcpy(sun.rise, sr + 11, 5); sun.rise[5] = 0; }
    if (strlen(ssr) >= 16) { memcpy(sun.set, ssr + 11, 5); sun.set[5] = 0; }
    sun.ok = true;
    return true;
}

bool AppClaudeMeter::_fetch_net(NetSnap& out)
{
    const std::uint32_t t0 = HAL::SysCtrl().millis();
    auto resp = HAL::Http().get("http://www.gstatic.com/generate_204", "", 8);
    const std::uint32_t dt = HAL::SysCtrl().millis() - t0;
    if (resp.http_code <= 0) { out.err = "no link"; return false; }
    out.latency_ms = (int)dt;
    out.ok = true;
    return true;
}

/* ----------------- Uptime monitor / Pet / Screensaver ----------------- */

void AppClaudeMeter::_build_uptime_view()
{
    _up_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_up_container);
    lv_obj_set_size(_up_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_up_container, 0, 0);
    lv_obj_set_style_bg_color(_up_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_up_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_up_container, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* title = lv_label_create(_up_container);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "UPTIME");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    for (int i = 0; i < 5; ++i) {
        const int y = 44 + i * 34;
        _up_dots[i] = lv_obj_create(_up_container);
        lv_obj_remove_style_all(_up_dots[i]);
        lv_obj_set_size(_up_dots[i], 16, 16);
        lv_obj_set_style_radius(_up_dots[i], 8, 0);
        lv_obj_set_style_bg_color(_up_dots[i], COLOR_BAR_BG, 0);
        lv_obj_set_style_bg_opa(_up_dots[i], LV_OPA_COVER, 0);
        lv_obj_align(_up_dots[i], LV_ALIGN_TOP_LEFT, 14, y);
        lv_obj_add_flag(_up_dots[i], LV_OBJ_FLAG_HIDDEN);

        _up_rows[i] = lv_label_create(_up_container);
        lv_obj_set_style_text_color(_up_rows[i], COLOR_FG, 0);
        lv_obj_set_style_text_font(_up_rows[i], &lv_font_montserrat_14, 0);
        lv_label_set_text(_up_rows[i], "");
        lv_obj_align(_up_rows[i], LV_ALIGN_TOP_LEFT, 40, y);
    }
}

void AppClaudeMeter::_update_uptime()
{
    if (!_up_rows[0]) return;
    UpSnap u;
    {
        std::lock_guard<std::mutex> lock(_data_mutex);
        u = _up;
    }
    if (!u.ok) {
        lv_label_set_text(_up_rows[0], u.err.empty() ? "..." : u.err.c_str());
        lv_obj_add_flag(_up_dots[0], LV_OBJ_FLAG_HIDDEN);
        for (int i = 1; i < 5; ++i) { lv_label_set_text(_up_rows[i], ""); lv_obj_add_flag(_up_dots[i], LV_OBJ_FLAG_HIDDEN); }
        return;
    }
    for (int i = 0; i < 5; ++i) {
        if (i >= u.count) {
            lv_label_set_text(_up_rows[i], "");
            lv_obj_add_flag(_up_dots[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(_up_dots[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(_up_dots[i], u.sites[i].up ? COLOR_OK : COLOR_DANGER, 0);
        char b[48];
        if (u.sites[i].up) std::snprintf(b, sizeof(b), "%-15s %dms", u.sites[i].host, u.sites[i].ms);
        else std::snprintf(b, sizeof(b), "%s", u.sites[i].host);
        lv_label_set_text(_up_rows[i], b);
    }
}

bool AppClaudeMeter::_fetch_uptime(UpSnap& out)
{
    const std::string& cfg = HAL::SysCfg().getConfig().uptimeUrls;
    if (cfg.empty()) { out.err = "set URLs"; return false; }

    // Split on whitespace/commas, up to 5 URLs.
    std::string urls[5];
    int n = 0;
    size_t i = 0;
    while (i < cfg.size() && n < 5) {
        while (i < cfg.size() && (cfg[i] == ' ' || cfg[i] == ',' || cfg[i] == '\n' || cfg[i] == '\r' || cfg[i] == '\t')) ++i;
        size_t start = i;
        while (i < cfg.size() && cfg[i] != ' ' && cfg[i] != ',' && cfg[i] != '\n' && cfg[i] != '\r' && cfg[i] != '\t') ++i;
        if (i > start) urls[n++] = cfg.substr(start, i - start);
    }
    if (n == 0) { out.err = "set URLs"; return false; }

    for (int k = 0; k < n; ++k) {
        // host = between "://" and the next '/'
        std::string host = urls[k];
        size_t p = host.find("://");
        if (p != std::string::npos) host = host.substr(p + 3);
        size_t slash = host.find('/');
        if (slash != std::string::npos) host = host.substr(0, slash);
        std::snprintf(out.sites[k].host, sizeof(out.sites[k].host), "%s", host.c_str());

        const std::uint32_t t0 = HAL::SysCtrl().millis();
        auto resp = HAL::Http().get(urls[k], "", 12); // allow time for TLS handshake
        const std::uint32_t dt = HAL::SysCtrl().millis() - t0;
        out.sites[k].up = (resp.http_code >= 200 && resp.http_code < 400);
        out.sites[k].ms = (int)dt;
    }
    out.count = n;
    out.ok = true;
    return true;
}

void AppClaudeMeter::_build_pet_view()
{
    _pet_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_pet_container);
    lv_obj_set_size(_pet_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_pet_container, 0, 0);
    lv_obj_set_style_bg_color(_pet_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_pet_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_pet_container, LV_OBJ_FLAG_CLICKABLE);

    // Round face that gently bobs.
    lv_obj_t* face = lv_obj_create(_pet_container);
    lv_obj_remove_style_all(face);
    lv_obj_set_size(face, 130, 120);
    lv_obj_set_style_radius(face, 60, 0);
    lv_obj_set_style_bg_color(face, COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(face, LV_OPA_COVER, 0);
    lv_obj_align(face, LV_ALIGN_CENTER, 0, 0);

    auto eye = [&](int dx) {
        lv_obj_t* e = lv_obj_create(face);
        lv_obj_remove_style_all(e);
        lv_obj_set_size(e, 18, 18);
        lv_obj_set_style_radius(e, 9, 0);
        lv_obj_set_style_bg_color(e, lv_color_hex(0x101010), 0);
        lv_obj_set_style_bg_opa(e, LV_OPA_COVER, 0);
        lv_obj_align(e, LV_ALIGN_CENTER, dx, -14);
        return e;
    };
    lv_obj_t* eyeL = eye(-26);
    lv_obj_t* eyeR = eye(26);

    lv_obj_t* mouth = lv_obj_create(face);
    lv_obj_remove_style_all(mouth);
    lv_obj_set_size(mouth, 44, 10);
    lv_obj_set_style_radius(mouth, 5, 0);
    lv_obj_set_style_bg_color(mouth, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(mouth, LV_OPA_COVER, 0);
    lv_obj_align(mouth, LV_ALIGN_CENTER, 0, 26);

    // Bob the whole face up and down.
    static lv_anim_t bob;
    lv_anim_init(&bob);
    lv_anim_set_var(&bob, face);
    lv_anim_set_exec_cb(&bob, [](void* o, int32_t v) { lv_obj_align((lv_obj_t*)o, LV_ALIGN_CENTER, 0, v); });
    lv_anim_set_values(&bob, -14, 14);
    lv_anim_set_duration(&bob, 900);
    lv_anim_set_playback_duration(&bob, 900);
    lv_anim_set_repeat_count(&bob, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&bob);

    // Blink: squash both eyes periodically.
    static lv_anim_t blinkL, blinkR;
    auto blink = [](lv_anim_t* a, lv_obj_t* e) {
        lv_anim_init(a);
        lv_anim_set_var(a, e);
        lv_anim_set_exec_cb(a, [](void* o, int32_t v) { lv_obj_set_height((lv_obj_t*)o, v); });
        lv_anim_set_values(a, 18, 2);
        lv_anim_set_duration(a, 120);
        lv_anim_set_playback_duration(a, 120);
        lv_anim_set_repeat_count(a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_repeat_delay(a, 2600);
        lv_anim_start(a);
    };
    blink(&blinkL, eyeL);
    blink(&blinkR, eyeR);
}

void AppClaudeMeter::_build_saver_view()
{
    _saver_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_saver_container);
    lv_obj_set_size(_saver_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_saver_container, 0, 0);
    lv_obj_set_style_bg_color(_saver_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_saver_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_saver_container, LV_OBJ_FLAG_CLICKABLE);

    // Falling "matrix"/starfield dots, each on its own looping y animation.
    static lv_anim_t star_anim[kStarN];
    for (int i = 0; i < kStarN; ++i) {
        _stars[i] = lv_obj_create(_saver_container);
        lv_obj_remove_style_all(_stars[i]);
        const int sz = 2 + (i % 3);
        lv_obj_set_size(_stars[i], sz, sz + 2);
        lv_obj_set_style_radius(_stars[i], 1, 0);
        lv_obj_set_style_bg_color(_stars[i], (i % 4 == 0) ? COLOR_ACCENT : lv_color_hex(0x66FF99), 0);
        lv_obj_set_style_bg_opa(_stars[i], LV_OPA_COVER, 0);
        lv_obj_set_x(_stars[i], (i * 71 + 13) % (SCREEN_W - 6));

        lv_anim_init(&star_anim[i]);
        lv_anim_set_var(&star_anim[i], _stars[i]);
        lv_anim_set_exec_cb(&star_anim[i], [](void* o, int32_t v) { lv_obj_set_y((lv_obj_t*)o, v); });
        lv_anim_set_values(&star_anim[i], -8, SCREEN_H + 8);
        lv_anim_set_duration(&star_anim[i], 1500 + (i % 6) * 450);
        lv_anim_set_delay(&star_anim[i], i * 160);
        lv_anim_set_repeat_count(&star_anim[i], LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&star_anim[i]);
    }
}

void AppClaudeMeter::_wake()
{
    _display_on = true;
    _pinned = false;
    HAL::Backlight().on();
    _show_screen(0); // wake back to the first screen (clock)
}

void AppClaudeMeter::_handle_tap()
{
    if (_booting) return; // ignore taps until the clock is up
    const std::uint32_t now = HAL::SysCtrl().millis();
    const bool is_double = (now - _last_click_ms) < DOUBLE_TAP_MS;
    _last_click_ms = now;
    _last_interaction_ms = now;

    // If the display was asleep, the tap just wakes it back to the clock.
    if (!_display_on) {
        _wake();
        return;
    }

    HAL::Backlight().on();

    if (is_double) {
        // Double-tap pins the current screen: stays on, never sleeps, no cycle.
        _pinned = true;
        return;
    }

    // Single tap cycles to the next screen and clears any pin.
    _pinned = false;
    if (!_screens.empty()) {
        _show_screen((_screen_idx + 1) % (int)_screens.size());
    }
}

void AppClaudeMeter::_on_root_clicked(lv_event_t* e)
{
    auto* self = static_cast<AppClaudeMeter*>(lv_event_get_user_data(e));
    if (!self) return;
    self->_handle_tap();
}

/* ------------------------------ HTTP fetch ------------------------------ */

void AppClaudeMeter::_start_fetch_thread()
{
    if (_fetch_thread.joinable()) return;
    _fetch_stop.store(false);
    _fetch_thread = std::thread([this] { _fetch_loop(); });
}

void AppClaudeMeter::_stop_fetch_thread()
{
    _fetch_stop.store(true);
    if (_fetch_thread.joinable()) _fetch_thread.join();
}

void AppClaudeMeter::_fetch_loop()
{
    using BL = hal_components::BacklightBase;
    bool prev_err = false;     // was the last poll an error? (pulse only on entry)
    bool prev_limit = false;   // was usage already over the danger line?

    // First-fetch attempt on startup, then poll every FETCH_PERIOD_SEC.
    while (!_fetch_stop.load()) {
        Snapshot fresh;
        bool ok = _fetch_once(fresh);

        {
            std::lock_guard<std::mutex> lock(_snapshot_mutex);
            if (ok) {
                _snapshot = fresh;
            } else {
                _snapshot.state = Fetch_Err;
                _snapshot.last_err = fresh.last_err;
            }
        }

        // Backlight notifications (no-op on platforms without a backlight).
        // Success pulses on every poll; "limit reached" supersedes it when
        // usage crosses the danger line; errors pulse once per error episode.
        if (ok) {
            const bool limit = (fresh.pct_five_hour >= DANGER_THRESHOLD) ||
                               (fresh.pct_seven_day >= DANGER_THRESHOLD);
            if (limit && !prev_limit) {
                HAL::Backlight().notify(BL::Notify_LimitReached);
            } else {
                HAL::Backlight().notify(BL::Notify_FetchOk);
            }
            prev_limit = limit;
            prev_err = false;
        } else {
            if (!prev_err) HAL::Backlight().notify(BL::Notify_FetchErr);
            prev_err = true;
            prev_limit = false;
        }

        if (ok) {
            mclog::tagInfo(getAppInfo().name, "fetch ok: 5h={:.1f}% 7d={:.1f}%",
                           fresh.pct_five_hour, fresh.pct_seven_day);
        } else {
            mclog::tagWarn(getAppInfo().name, "fetch err: {}", fresh.last_err);
        }

        // Back off 5 minutes after a good fetch, but retry every 5 seconds
        // while we're erroring (typically waiting on WiFi to associate).
        int wait_sec = ok ? FETCH_PERIOD_SEC : 5;
        for (int i = 0; i < wait_sec * 4 && !_fetch_stop.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
}

bool AppClaudeMeter::_fetch_once(Snapshot& out)
{
    auto& sys = HAL::Get();
    if (!sys.SysCfg().isClaudeReady()) {
        out.last_err = "no cfg";
        return false;
    }
    const auto& cfg = sys.SysCfg().getConfig();

    std::string url = trim_trailing_slash(cfg.claudeBase) + "/usage";
    auto resp = HAL::Http().get(url, cfg.claudeBearer, FETCH_TIMEOUT_SEC);

    if (resp.http_code == 0) {
        out.last_err = resp.error.empty() ? std::string("net err") : resp.error;
        return false;
    }
    if (resp.http_code != 200) {
        char buf[40];
        std::snprintf(buf, sizeof(buf), "HTTP %d", resp.http_code);
        out.last_err = buf;
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, resp.body) != DeserializationError::Ok) {
        out.last_err = "bad json";
        return false;
    }

    if (doc["five_hour"]["utilization"].is<float>()) {
        out.pct_five_hour = doc["five_hour"]["utilization"].as<float>();
    }
    if (doc["seven_day"]["utilization"].is<float>()) {
        out.pct_seven_day = doc["seven_day"]["utilization"].as<float>();
    }

    if (out.pct_five_hour < 0.0f && out.pct_seven_day < 0.0f) {
        out.last_err = "no fields";
        return false;
    }

    out.state = Fetch_OK;
    return true;
}

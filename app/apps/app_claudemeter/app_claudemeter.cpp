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
#include <chrono>
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

    _start_fetch_thread();
    _start_weather_thread();
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

    // Boot splash (shown until the clock syncs via SNTP).
    _build_boot_screen();

    // Build each screen's container, then register it. Tap cycles in this order.
    _build_clock_view();
    _build_meter_view();
    _build_weather_view();

    _register_screen("clock", _clock_container, [this] { _update_clock(); });
    _register_screen("meter", _meter_container, [this] { _update_meter(); });
    _register_screen("weather", _weather_container, [this] { _update_weather(); });
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
                                      std::function<void()> update)
{
    _screens.push_back({name, container, std::move(update)});
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

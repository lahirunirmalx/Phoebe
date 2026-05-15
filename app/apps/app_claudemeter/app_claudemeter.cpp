/**
 * @file app_claudemeter.cpp
 * @brief Claude usage meter app -- clock + meter views, click toggles.
 *        Real-time data: a background thread polls <base>/usage and updates
 *        a mutex-guarded snapshot. The UI reads the snapshot each tick.
 */
#include "app_claudemeter.h"
#include "hal/hal.h"
#include <ArduinoJson.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <mooncake_log.h>
#include <lvgl.h>

using namespace mooncake;

namespace {

constexpr int SCREEN_W = 144;
constexpr int SCREEN_H = 168;

// Clock canvas geometry (square)
constexpr int CLOCK_CANVAS_W = 110;
constexpr int CLOCK_CANVAS_H = 110;

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

lv_color_t bar_color_for(float pct)
{
    if (pct >= DANGER_THRESHOLD) return COLOR_DANGER;
    if (pct >= WARN_THRESHOLD) return COLOR_WARN;
    return COLOR_OK;
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
    _show_view(_view);
    _update_clock();
    _update_meter();

    _start_fetch_thread();
}

void AppClaudeMeter::onRunning()
{
    lv_timer_handler();

    const std::uint32_t now_ms = HAL::SysCtrl().millis();
    if (now_ms - _last_tick_ms < 500) return;
    _last_tick_ms = now_ms;

    if (_view == VIEW_CLOCK) {
        _update_clock();
    } else {
        // If we have no live data yet, drift the mock values so the bars
        // remain visibly alive during development.
        Snapshot snap;
        {
            std::lock_guard<std::mutex> lock(_snapshot_mutex);
            snap = _snapshot;
        }
        if (snap.state != Fetch_OK) {
            _mock_pct_five_hour += 0.7f;
            if (_mock_pct_five_hour > 100.0f) _mock_pct_five_hour = 0.0f;
            _mock_pct_seven_day += 0.3f;
            if (_mock_pct_seven_day > 100.0f) _mock_pct_seven_day = 0.0f;
        }
        _update_meter();
    }
}

void AppClaudeMeter::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _stop_fetch_thread();

    if (_clock_anim_arc) {
        lv_anim_delete(_clock_anim_arc, NULL);
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

    _build_clock_view();
    _build_meter_view();
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
    // Analog clock canvas at the top; digital time and date at the bottom.
    _clock_canvas_buf = new std::uint8_t[CLOCK_CANVAS_W * CLOCK_CANVAS_H * 2];
    _clock_canvas = lv_canvas_create(_clock_container);
    lv_canvas_set_buffer(_clock_canvas, _clock_canvas_buf, CLOCK_CANVAS_W, CLOCK_CANVAS_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_align(_clock_canvas, LV_ALIGN_TOP_MID, 0, 18);

    _clock_time_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_time_label, COLOR_FG, 0);
    lv_obj_set_style_text_font(_clock_time_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(_clock_time_label, "00:00");
    lv_obj_align(_clock_time_label, LV_ALIGN_BOTTOM_MID, 0, -22);

    _clock_date_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_date_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_clock_date_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_clock_date_label, "");
    lv_obj_align(_clock_date_label, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void AppClaudeMeter::_build_clock_digital()
{
    // Big HH:MM centered; smaller :SS below; date at the bottom.
    _clock_time_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_time_label, COLOR_FG, 0);
    lv_obj_set_style_text_font(_clock_time_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(_clock_time_label, "00:00");
    lv_obj_align(_clock_time_label, LV_ALIGN_CENTER, 0, -8);

    _clock_sec_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_sec_label, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(_clock_sec_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_clock_sec_label, ":00");
    lv_obj_align_to(_clock_sec_label, _clock_time_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

    _clock_date_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_date_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_clock_date_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_clock_date_label, "");
    lv_obj_align(_clock_date_label, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void AppClaudeMeter::_build_clock_animated()
{
    // Rotating accent arc behind the time. Driven by an lv_anim_t that
    // sweeps the arc start angle continuously, independent of wall time.
    _clock_anim_arc = lv_arc_create(_clock_container);
    lv_obj_set_size(_clock_anim_arc, 110, 110);
    lv_obj_align(_clock_anim_arc, LV_ALIGN_CENTER, 0, -4);
    lv_obj_remove_style(_clock_anim_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(_clock_anim_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_bg_angles(_clock_anim_arc, 0, 360);
    lv_arc_set_angles(_clock_anim_arc, 0, 60);
    lv_obj_set_style_arc_color(_clock_anim_arc, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_color(_clock_anim_arc, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(_clock_anim_arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_clock_anim_arc, 6, LV_PART_INDICATOR);

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
    lv_obj_set_style_text_font(_clock_time_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(_clock_time_label, "00:00");
    lv_obj_align(_clock_time_label, LV_ALIGN_CENTER, 0, -4);

    _clock_date_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_date_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_clock_date_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_clock_date_label, "");
    lv_obj_align(_clock_date_label, LV_ALIGN_BOTTOM_MID, 0, -4);
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
    constexpr int CW = 124;
    constexpr int CH = 50;
    _clock_face_canvas_buf = new std::uint8_t[CW * CH * 2];
    _clock_face_canvas = lv_canvas_create(_clock_container);
    lv_canvas_set_buffer(_clock_face_canvas, _clock_face_canvas_buf, CW, CH,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_align(_clock_face_canvas, LV_ALIGN_CENTER, 0, -10);

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
    constexpr int CW = 128;
    constexpr int CH = 32;
    _clock_face_canvas_buf = new std::uint8_t[CW * CH * 2];
    _clock_face_canvas = lv_canvas_create(_clock_container);
    lv_canvas_set_buffer(_clock_face_canvas, _clock_face_canvas_buf, CW, CH,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_align(_clock_face_canvas, LV_ALIGN_CENTER, 0, -10);

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
    lv_obj_align(_meter_title_label, LV_ALIGN_TOP_MID, 0, 6);

    // 5-hour row
    _meter_h5_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_color(_meter_h5_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_meter_h5_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_meter_h5_label, "5H");
    lv_obj_align(_meter_h5_label, LV_ALIGN_TOP_LEFT, 6, 36);

    _meter_h5_pct_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_font(_meter_h5_pct_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(_meter_h5_pct_label, "--");
    lv_obj_align(_meter_h5_pct_label, LV_ALIGN_TOP_RIGHT, -6, 28);

    _meter_h5_bar = lv_bar_create(_meter_container);
    lv_obj_set_size(_meter_h5_bar, SCREEN_W - 16, 10);
    lv_obj_align(_meter_h5_bar, LV_ALIGN_TOP_LEFT, 8, 60);
    lv_bar_set_range(_meter_h5_bar, 0, 100);
    lv_obj_set_style_bg_color(_meter_h5_bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_radius(_meter_h5_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_meter_h5_bar, 2, LV_PART_INDICATOR);

    // 7-day row
    _meter_d7_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_color(_meter_d7_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_meter_d7_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_meter_d7_label, "7D");
    lv_obj_align(_meter_d7_label, LV_ALIGN_TOP_LEFT, 6, 92);

    _meter_d7_pct_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_font(_meter_d7_pct_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(_meter_d7_pct_label, "--");
    lv_obj_align(_meter_d7_pct_label, LV_ALIGN_TOP_RIGHT, -6, 84);

    _meter_d7_bar = lv_bar_create(_meter_container);
    lv_obj_set_size(_meter_d7_bar, SCREEN_W - 16, 10);
    lv_obj_align(_meter_d7_bar, LV_ALIGN_TOP_LEFT, 8, 116);
    lv_bar_set_range(_meter_d7_bar, 0, 100);
    lv_obj_set_style_bg_color(_meter_d7_bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_radius(_meter_d7_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_meter_d7_bar, 2, LV_PART_INDICATOR);

    _meter_status_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_color(_meter_status_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_meter_status_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_meter_status_label, "");
    lv_obj_align(_meter_status_label, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void AppClaudeMeter::_show_view(View v)
{
    _view = v;
    if (v == VIEW_CLOCK) {
        lv_obj_clear_flag(_clock_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_meter_container, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_clock_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_meter_container, LV_OBJ_FLAG_HIDDEN);
    }
}

void AppClaudeMeter::_toggle_view()
{
    _show_view(_view == VIEW_CLOCK ? VIEW_METER : VIEW_CLOCK);
    if (_view == VIEW_CLOCK) {
        _update_clock();
    } else {
        _update_meter();
    }
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
    // visual identity and must NOT flip green/orange/red with the threshold.
    // Other faces use the usual threshold colour scheme.
    bool themed = (_watch_face == WF_Seg7 || _watch_face == WF_VFD);
    lv_color_t c = themed ? lv_obj_get_style_bg_color(_clock_5h_bar, LV_PART_INDICATOR)
                          : bar_color_for(p5);

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
    if (!_clock_canvas) return;

    char time_buf[16];
    std::snprintf(time_buf, sizeof(time_buf), "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);
    lv_label_set_text(_clock_time_label, time_buf);

    char date_buf[24];
    std::snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d",
                  tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday);
    lv_label_set_text(_clock_date_label, date_buf);

    lv_canvas_fill_bg(_clock_canvas, COLOR_BG, LV_OPA_COVER);

    const int cx = CLOCK_CANVAS_W / 2;
    const int cy = CLOCK_CANVAS_H / 2;
    const int hour = tm_info.tm_hour % 12;
    const int minute = tm_info.tm_min;
    const int second = tm_info.tm_sec;

    const float hour_angle = (hour + minute / 60.0f) * 30.0f * (float)M_PI / 180.0f;
    const float minute_angle = (minute + second / 60.0f) * 6.0f * (float)M_PI / 180.0f;
    const float second_angle = second * 6.0f * (float)M_PI / 180.0f;

    lv_layer_t layer;
    lv_canvas_init_layer(_clock_canvas, &layer);

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = COLOR_FG;

    auto draw_hand = [&](float angle, int length, int width) {
        dsc.width = width;
        dsc.p1.x = cx;
        dsc.p1.y = cy;
        dsc.p2.x = cx + (int)(length * std::cos(angle - (float)M_PI / 2.0f));
        dsc.p2.y = cy + (int)(length * std::sin(angle - (float)M_PI / 2.0f));
        lv_draw_line(&layer, &dsc);
    };

    draw_hand(hour_angle, 24, 5);
    draw_hand(minute_angle, 36, 3);
    dsc.color = COLOR_ACCENT;
    draw_hand(second_angle, 44, 2);

    lv_canvas_finish_layer(_clock_canvas, &layer);
}

void AppClaudeMeter::_update_clock_digital(const struct tm& tm_info)
{
    if (!_clock_time_label) return;

    char buf[16];
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
    char buf[16];
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

        constexpr int CW = 124;
        constexpr int CH = 50;
        constexpr int DW = 20;
        constexpr int DH = 44;
        constexpr int T = 4;
        constexpr int GAP = 4;
        constexpr int COLON_W = 10;

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

    char buf[24];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                  tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday);
    lv_label_set_text(_clock_date_label, buf);
}

void AppClaudeMeter::_update_clock_vfd(const struct tm& tm_info)
{
    if (!_clock_face_canvas) return;

    if (tm_info.tm_sec != _clock_last_sec) {
        _clock_last_sec = tm_info.tm_sec;

        constexpr int CW = 128;
        constexpr int CH = 32;
        constexpr int DOT = 3;
        constexpr int PITCH = 4;
        constexpr int GLYPH_W = 5 * PITCH; // 20
        constexpr int GLYPH_GAP = 4;
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

    char buf[24];
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

    char buf[8];
    lv_color_t h5c = bar_color_for(p5);
    std::snprintf(buf, sizeof(buf), "%d%%", (int)(p5 + 0.5f));
    lv_label_set_text(_meter_h5_pct_label, buf);
    lv_obj_set_style_text_color(_meter_h5_pct_label, h5c, 0);
    lv_bar_set_value(_meter_h5_bar, (int)(p5 + 0.5f), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_meter_h5_bar, h5c, LV_PART_INDICATOR);

    lv_color_t d7c = bar_color_for(p7);
    std::snprintf(buf, sizeof(buf), "%d%%", (int)(p7 + 0.5f));
    lv_label_set_text(_meter_d7_pct_label, buf);
    lv_obj_set_style_text_color(_meter_d7_pct_label, d7c, 0);
    lv_bar_set_value(_meter_d7_bar, (int)(p7 + 0.5f), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_meter_d7_bar, d7c, LV_PART_INDICATOR);

    lv_label_set_text(_meter_status_label, status_text);
}

void AppClaudeMeter::_on_root_clicked(lv_event_t* e)
{
    auto* self = static_cast<AppClaudeMeter*>(lv_event_get_user_data(e));
    if (!self) return;
    self->_toggle_view();
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

        if (ok) {
            mclog::tagInfo(getAppInfo().name, "fetch ok: 5h={:.1f}% 7d={:.1f}%",
                           fresh.pct_five_hour, fresh.pct_seven_day);
        } else {
            mclog::tagWarn(getAppInfo().name, "fetch err: {}", fresh.last_err);
        }

        // Sleep in short slices so stop is responsive.
        for (int i = 0; i < FETCH_PERIOD_SEC * 4 && !_fetch_stop.load(); ++i) {
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
        char buf[24];
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

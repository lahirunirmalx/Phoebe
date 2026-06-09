/**
 * @file ui_common.h
 * @brief Shared theme + drawing helpers for the Phoebe screen apps.
 *
 * Colors, thresholds, weather-code helpers, the usage ring, and the 7-segment /
 * VFD dot-matrix fonts -- everything the individual app classes draw with. All
 * symbols are inline / const (internal linkage) so the header is safe to include
 * from every app translation unit.
 */
#pragma once

#include <cstdint>
#include <lvgl.h>
#include <string>

namespace ui {

// Panel size (overridden per-platform: the Mini TV builds with -DPHOEBE_SCREEN_W=240).
#ifndef PHOEBE_SCREEN_W
#define PHOEBE_SCREEN_W 144
#endif
#ifndef PHOEBE_SCREEN_H
#define PHOEBE_SCREEN_H 168
#endif
constexpr int SCREEN_W = PHOEBE_SCREEN_W;
constexpr int SCREEN_H = PHOEBE_SCREEN_H;

// Display sleep + single-touch gesture bands (used by the navigator).
constexpr std::uint32_t DISPLAY_SLEEP_MS = 5 * 60 * 1000;
constexpr std::uint32_t TAP_MAX_MS = 400;   // < this  -> tap (next app)
constexpr std::uint32_t PIN_MIN_MS = 400;   // [MIN,MAX) -> hold (toggle pin)
constexpr std::uint32_t PIN_MAX_MS = 2500;  // >= this -> reserved for the 3s portal hold

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
const lv_color_t COLOR_5H = LV_COLOR_MAKE(0x33, 0xC8, 0xFF); // cyan
const lv_color_t COLOR_7D = LV_COLOR_MAKE(0xFF, 0xC0, 0x40); // amber
const lv_color_t COLOR_SUN = LV_COLOR_MAKE(0xFF, 0xD2, 0x40);
const lv_color_t COLOR_CLOUD = LV_COLOR_MAKE(0xCA, 0xD2, 0xDE);
const lv_color_t COLOR_RAIN = LV_COLOR_MAKE(0x55, 0xAA, 0xFF);

enum WxCat { WX_CLEAR = 0, WX_CLOUD, WX_RAIN };

inline WxCat wx_category(int code)
{
    if (code <= 1) return WX_CLEAR;
    if (code == 2 || code == 3 || code == 45 || code == 48) return WX_CLOUD;
    return WX_RAIN;
}

inline const char* wx_text(int code)
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

inline lv_color_t metric_color(float pct, lv_color_t base)
{
    if (pct >= DANGER_THRESHOLD) return COLOR_DANGER;
    return base;
}

inline lv_color_t aqi_color(int aqi)
{
    if (aqi < 0) return COLOR_LABEL_DIM;
    if (aqi <= 40) return COLOR_OK;
    if (aqi <= 80) return COLOR_WARN;
    return COLOR_DANGER;
}

inline const char* aqi_text(int aqi)
{
    if (aqi < 0) return "--";
    if (aqi <= 20) return "Good";
    if (aqi <= 40) return "Fair";
    if (aqi <= 60) return "Moderate";
    if (aqi <= 80) return "Poor";
    if (aqi <= 100) return "Very poor";
    return "Extreme";
}

inline const char* const WDAY[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// 270-degree usage ring (no knob, not interactive).
inline lv_obj_t* make_ring(lv_obj_t* parent, int size, int width)
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

inline void set_ring(lv_obj_t* a, float pct, lv_color_t color)
{
    if (!a) return;
    lv_arc_set_value(a, (int)(pct + 0.5f));
    lv_obj_set_style_arc_color(a, color, LV_PART_INDICATOR);
}

/* ---------------- 7-segment font ---------------- */
// Bit i set if segment i is on. Order: a b c d e f g.
constexpr std::uint8_t SEG7_DIGITS[10] = {
    0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110,
    0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111,
};
const lv_color_t SEG7_ON = LV_COLOR_MAKE(0xFF, 0x30, 0x30);
const lv_color_t SEG7_OFF = LV_COLOR_MAKE(0x30, 0x05, 0x05);
const lv_color_t SEG7_BG = LV_COLOR_MAKE(0x0A, 0x00, 0x00);
const lv_color_t SEG7_BAR_BG = LV_COLOR_MAKE(0x1A, 0x00, 0x00);
const lv_color_t SEG7_DATE = LV_COLOR_MAKE(0xA0, 0x30, 0x30);

inline void draw_seg(lv_layer_t* layer, int x0, int y0, int x1, int y1, lv_color_t color)
{
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_color = color;
    r.bg_opa = LV_OPA_COVER;
    lv_area_t a = {x0, y0, x1 - 1, y1 - 1};
    lv_draw_rect(layer, &r, &a);
}

inline void draw_seg7_digit(lv_layer_t* layer, int x, int y, int digit, int dw, int dh, int t)
{
    std::uint8_t mask = (digit >= 0 && digit <= 9) ? SEG7_DIGITS[digit] : 0;
    int mid = dh / 2;
    auto seg = [&](int sx0, int sy0, int sx1, int sy1, int bit) {
        draw_seg(layer, x + sx0, y + sy0, x + sx1, y + sy1, (mask & (1 << bit)) ? SEG7_ON : SEG7_OFF);
    };
    seg(t, 0, dw - t, t, 0);
    seg(dw - t, t, dw, mid, 1);
    seg(dw - t, mid, dw, dh - t, 2);
    seg(t, dh - t, dw - t, dh, 3);
    seg(0, mid, t, dh - t, 4);
    seg(0, t, t, mid, 5);
    seg(t, mid - t / 2, dw - t, mid + t / 2, 6);
}

inline void draw_seg7_colon(lv_layer_t* layer, int x, int y, int w, int h, bool on)
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
 * 7 rows of 5 bits (MSB = leftmost). Index 0..9 == '0'..'9', 10 == ':'. */
constexpr std::uint8_t VFD_FONT[11][7] = {
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E},
    {0x11, 0x11, 0x11, 0x1F, 0x01, 0x01, 0x01},
    {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x11, 0x0E},
    {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E},
    {0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00},
};
const lv_color_t VFD_BG = LV_COLOR_MAKE(0x00, 0x08, 0x10);
const lv_color_t VFD_ON = LV_COLOR_MAKE(0x66, 0xFF, 0xCC);
const lv_color_t VFD_OFF = LV_COLOR_MAKE(0x0E, 0x18, 0x18);
const lv_color_t VFD_DIM = LV_COLOR_MAKE(0x3F, 0xAA, 0x88);

inline void draw_vfd_glyph(lv_layer_t* layer, int x, int y, int idx, int dot, int pitch)
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

} // namespace ui

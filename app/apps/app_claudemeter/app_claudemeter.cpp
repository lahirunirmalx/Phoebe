/**
 * @file app_claudemeter.cpp
 * @brief Claude usage meter app -- clock + meter views, click toggles.
 */
#include "app_claudemeter.h"
#include "hal/hal.h"
#include <cmath>
#include <cstdio>
#include <ctime>
#include <mooncake_log.h>
#include <lvgl.h>

using namespace mooncake;

namespace {

constexpr int SCREEN_W = 144;
constexpr int SCREEN_H = 168;

// Clock canvas geometry (square, centered)
constexpr int CLOCK_CANVAS_W = 110;
constexpr int CLOCK_CANVAS_H = 110;

// Color thresholds for the meter bars
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

} // namespace

AppClaudeMeter::AppClaudeMeter()
{
    setAppInfo().name = "AppClaudeMeter";
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

    _build_ui();
    _show_view(_view);
    _update_clock();
    _update_meter();
}

void AppClaudeMeter::onRunning()
{
    lv_timer_handler();

    const std::uint32_t now_ms = HAL::SysCtrl().millis();
    if (now_ms - _last_tick_ms < 500) {
        return;
    }
    _last_tick_ms = now_ms;

    if (_view == VIEW_CLOCK) {
        _update_clock();
    } else {
        // Drift the mock usage values so the bars visibly change.
        _pct_five_hour += 0.7f;
        if (_pct_five_hour > 100.0f) _pct_five_hour = 0.0f;
        _pct_seven_day += 0.3f;
        if (_pct_seven_day > 100.0f) _pct_seven_day = 0.0f;
        _update_meter();
    }
}

void AppClaudeMeter::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    if (_root) {
        lv_obj_delete(_root);
        _root = nullptr;
    }
    delete[] _clock_canvas_buf;
    _clock_canvas_buf = nullptr;
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

void AppClaudeMeter::_build_clock_view()
{
    _clock_container = lv_obj_create(_root);
    lv_obj_remove_style_all(_clock_container);
    lv_obj_set_size(_clock_container, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_clock_container, 0, 0);
    lv_obj_set_style_bg_color(_clock_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_clock_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_clock_container, LV_OBJ_FLAG_CLICKABLE);

    _clock_time_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_time_label, COLOR_FG, 0);
    lv_obj_set_style_text_font(_clock_time_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(_clock_time_label, "00:00");
    lv_obj_align(_clock_time_label, LV_ALIGN_TOP_MID, 0, 6);

    _clock_date_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_date_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_clock_date_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_clock_date_label, "");
    lv_obj_align(_clock_date_label, LV_ALIGN_TOP_MID, 0, 32);

    _clock_canvas_buf = new std::uint8_t[CLOCK_CANVAS_W * CLOCK_CANVAS_H * 2];
    _clock_canvas = lv_canvas_create(_clock_container);
    lv_canvas_set_buffer(_clock_canvas, _clock_canvas_buf, CLOCK_CANVAS_W, CLOCK_CANVAS_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_align(_clock_canvas, LV_ALIGN_CENTER, 0, 8);

    _clock_hint_label = lv_label_create(_clock_container);
    lv_obj_set_style_text_color(_clock_hint_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_clock_hint_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_clock_hint_label, "click \xE2\x86\x92 meter");
    lv_obj_align(_clock_hint_label, LV_ALIGN_BOTTOM_MID, 0, -4);
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

    _meter_hint_label = lv_label_create(_meter_container);
    lv_obj_set_style_text_color(_meter_hint_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_meter_hint_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_meter_hint_label, "click \xE2\x86\x92 clock");
    lv_obj_align(_meter_hint_label, LV_ALIGN_BOTTOM_MID, 0, -4);
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
    if (!_clock_canvas) return;

    time_t now;
    struct tm* tm_info;
    time(&now);
    tm_info = localtime(&now);
    if (!tm_info) return;

    char time_buf[16];
    std::snprintf(time_buf, sizeof(time_buf), "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
    lv_label_set_text(_clock_time_label, time_buf);

    char date_buf[24];
    std::snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d",
                  tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday);
    lv_label_set_text(_clock_date_label, date_buf);

    // Repaint analog face
    lv_canvas_fill_bg(_clock_canvas, COLOR_BG, LV_OPA_COVER);

    const int cx = CLOCK_CANVAS_W / 2;
    const int cy = CLOCK_CANVAS_H / 2;
    const int hour = tm_info->tm_hour % 12;
    const int minute = tm_info->tm_min;
    const int second = tm_info->tm_sec;

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

void AppClaudeMeter::_update_meter()
{
    if (!_meter_h5_bar) return;

    char buf[8];

    lv_color_t h5c = bar_color_for(_pct_five_hour);
    std::snprintf(buf, sizeof(buf), "%d%%", (int)(_pct_five_hour + 0.5f));
    lv_label_set_text(_meter_h5_pct_label, buf);
    lv_obj_set_style_text_color(_meter_h5_pct_label, h5c, 0);
    lv_bar_set_value(_meter_h5_bar, (int)(_pct_five_hour + 0.5f), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_meter_h5_bar, h5c, LV_PART_INDICATOR);

    lv_color_t d7c = bar_color_for(_pct_seven_day);
    std::snprintf(buf, sizeof(buf), "%d%%", (int)(_pct_seven_day + 0.5f));
    lv_label_set_text(_meter_d7_pct_label, buf);
    lv_obj_set_style_text_color(_meter_d7_pct_label, d7c, 0);
    lv_bar_set_value(_meter_d7_bar, (int)(_pct_seven_day + 0.5f), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_meter_d7_bar, d7c, LV_PART_INDICATOR);
}

void AppClaudeMeter::_on_root_clicked(lv_event_t* e)
{
    auto* self = static_cast<AppClaudeMeter*>(lv_event_get_user_data(e));
    if (!self) return;
    self->_toggle_view();
}

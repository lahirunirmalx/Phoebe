/**
 * @file app_clock.cpp
 * @brief See app_clock.h.
 */
#include "app_clock.h"

#include "apps/utils/data_service/data_service.h"
#include <cmath>
#include <cstdio>

using namespace ui;

AppClock::AppClock()
{
    setAppInfo().name = "clock";
}

AppClock::Face AppClock::resolve_face() const
{
    const auto& wf = HAL::SysCfg().getConfig().watchFace;
    if (wf == "digital") return F_Digital;
    if (wf == "animated") return F_Animated;
    if (wf == "seg7") return F_Seg7;
    if (wf == "vfd") return F_VFD;
    if (wf == "flip") return F_Flip;
    return F_Analog;
}

void AppClock::build(lv_obj_t* root)
{
    _face = resolve_face();
    build_5h_bar(root);
    switch (_face) {
        case F_Digital:  build_digital(root);  break;
        case F_Animated: build_animated(root); break;
        case F_Seg7:     build_seg7(root);     break;
        case F_VFD:      build_vfd(root);      break;
        case F_Flip:     build_flip(root);     break;
        case F_Analog:
        default:         build_analog(root);   break;
    }
}

void AppClock::teardown()
{
    _canvas = nullptr;
    delete[] _buf;
    _buf = nullptr;
}

/* ------------------------------- 5H bar -------------------------------- */

void AppClock::build_5h_bar(lv_obj_t* root)
{
    _bar_pct = lv_label_create(root);
    lv_obj_set_style_text_color(_bar_pct, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_bar_pct, &lv_font_montserrat_14, 0);
    lv_label_set_text(_bar_pct, "5H --");
    lv_obj_align(_bar_pct, LV_ALIGN_TOP_RIGHT, -4, 0);

    _bar = lv_bar_create(root);
    lv_obj_set_size(_bar, SCREEN_W - 64, 4);
    lv_obj_align(_bar, LV_ALIGN_TOP_LEFT, 4, 8);
    lv_bar_set_range(_bar, 0, 100);
    lv_obj_set_style_bg_color(_bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_radius(_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_bar, 1, LV_PART_INDICATOR);
}

void AppClock::update_5h_bar()
{
    if (!_bar) return;
    const appdata::ClaudeData snap = appdata::DataService::instance().claude();
    float p5;
    if (snap.state == appdata::Fetch_OK && snap.pct_five_hour >= 0.0f) {
        p5 = snap.pct_five_hour;
    } else {
        _mock5 += 0.7f; if (_mock5 > 100.0f) _mock5 = 0.0f;
        p5 = _mock5;
    }
    const bool themed = (_face == F_Seg7 || _face == F_VFD);
    lv_color_t c = themed ? lv_obj_get_style_bg_color(_bar, LV_PART_INDICATOR)
                          : metric_color(p5, COLOR_5H);
    lv_bar_set_value(_bar, (int)(p5 + 0.5f), LV_ANIM_OFF);
    if (!themed) lv_obj_set_style_bg_color(_bar, c, LV_PART_INDICATOR);
    char buf[12];
    std::snprintf(buf, sizeof(buf), "5H %d%%", (int)(p5 + 0.5f));
    lv_label_set_text(_bar_pct, buf);
    if (!themed) lv_obj_set_style_text_color(_bar_pct, c, 0);
}

/* -------------------------------- tick --------------------------------- */

void AppClock::tick()
{
    update_5h_bar();
    time_t now;
    time(&now);
    struct tm* t = localtime(&now);
    if (!t) return;
    switch (_face) {
        case F_Digital:  update_digital(*t);  break;
        case F_Animated: update_animated(*t); break;
        case F_Seg7:     update_seg7(*t);     break;
        case F_VFD:      update_vfd(*t);      break;
        case F_Flip:     update_flip(*t);     break;
        case F_Analog:
        default:         update_analog(*t);   break;
    }
}

/* ------------------------------- analog -------------------------------- */

void AppClock::build_analog(lv_obj_t* root)
{
    const int OFF = 8;
    const int R = (SCREEN_W < SCREEN_H ? SCREEN_W : SCREEN_H) / 2 - 10;

    lv_obj_t* dial = lv_arc_create(root);
    lv_obj_set_size(dial, R * 2, R * 2);
    lv_obj_align(dial, LV_ALIGN_CENTER, 0, OFF);
    lv_obj_remove_style(dial, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(dial, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_bg_angles(dial, 0, 360);
    lv_arc_set_angles(dial, 0, 0);
    lv_obj_set_style_arc_color(dial, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_width(dial, 3, LV_PART_MAIN);

    auto mk_hand = [&](lv_color_t color, int width) {
        lv_obj_t* l = lv_line_create(root);
        lv_obj_set_pos(l, 0, 0);
        lv_obj_set_style_line_color(l, color, 0);
        lv_obj_set_style_line_width(l, width, 0);
        lv_obj_set_style_line_rounded(l, true, 0);
        return l;
    };
    _hour_line = mk_hand(COLOR_FG, 8);
    _min_line = mk_hand(COLOR_FG, 5);
    _sec_line = mk_hand(COLOR_ACCENT, 3);

    lv_obj_t* hub = lv_obj_create(root);
    lv_obj_remove_style_all(hub);
    lv_obj_set_size(hub, 12, 12);
    lv_obj_set_style_radius(hub, 6, 0);
    lv_obj_set_style_bg_color(hub, COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
    lv_obj_align(hub, LV_ALIGN_CENTER, 0, OFF);

    _date = lv_label_create(root);
    lv_obj_set_style_text_color(_date, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_date, &lv_font_montserrat_14, 0);
    lv_label_set_text(_date, "");
    lv_obj_align(_date, LV_ALIGN_BOTTOM_MID, 0, -6);
}

void AppClock::update_analog(const struct tm& t)
{
    if (!_sec_line) return;
    char date_buf[40];
    std::snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    lv_label_set_text(_date, date_buf);

    const float cx = SCREEN_W / 2.0f;
    const float cy = SCREEN_H / 2.0f + 8;
    const int R = (SCREEN_W < SCREEN_H ? SCREEN_W : SCREEN_H) / 2 - 10;
    const int hour = t.tm_hour % 12, minute = t.tm_min, second = t.tm_sec;
    const float ha = (hour + minute / 60.0f) * 30.0f * (float)M_PI / 180.0f;
    const float ma = (minute + second / 60.0f) * 6.0f * (float)M_PI / 180.0f;
    const float sa = second * 6.0f * (float)M_PI / 180.0f;

    auto set_hand = [&](lv_obj_t* line, lv_point_precise_t* pts, float angle, float len) {
        pts[0].x = cx; pts[0].y = cy;
        pts[1].x = cx + len * std::sin(angle);
        pts[1].y = cy - len * std::cos(angle);
        lv_line_set_points(line, pts, 2);
    };
    set_hand(_hour_line, _hpts, ha, R * 0.50f);
    set_hand(_min_line, _mpts, ma, R * 0.74f);
    set_hand(_sec_line, _spts, sa, R * 0.90f);
}

/* ------------------------------- digital ------------------------------- */

void AppClock::build_digital(lv_obj_t* root)
{
    _time = lv_label_create(root);
    lv_obj_set_style_text_color(_time, COLOR_FG, 0);
    lv_obj_set_style_text_font(_time, &lv_font_montserrat_48, 0);
    lv_label_set_text(_time, "00:00");
    lv_obj_align(_time, LV_ALIGN_CENTER, 0, -20);

    _sec_lbl = lv_label_create(root);
    lv_obj_set_style_text_color(_sec_lbl, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(_sec_lbl, &lv_font_montserrat_24, 0);
    lv_label_set_text(_sec_lbl, ":00");
    lv_obj_align_to(_sec_lbl, _time, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

    _date = lv_label_create(root);
    lv_obj_set_style_text_color(_date, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_date, &lv_font_montserrat_14, 0);
    lv_label_set_text(_date, "");
    lv_obj_align(_date, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void AppClock::update_digital(const struct tm& t)
{
    if (!_time) return;
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    lv_label_set_text(_time, buf);
    if (_sec_lbl) { std::snprintf(buf, sizeof(buf), ":%02d", t.tm_sec); lv_label_set_text(_sec_lbl, buf); }
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    lv_label_set_text(_date, buf);
}

/* ------------------------------ animated ------------------------------- */

void AppClock::build_animated(lv_obj_t* root)
{
    _anim_arc = lv_arc_create(root);
    lv_obj_set_size(_anim_arc, 196, 196);
    lv_obj_align(_anim_arc, LV_ALIGN_CENTER, 0, -4);
    lv_obj_remove_style(_anim_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(_anim_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_bg_angles(_anim_arc, 0, 360);
    lv_arc_set_angles(_anim_arc, 0, 60);
    lv_obj_set_style_arc_color(_anim_arc, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_color(_anim_arc, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(_anim_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_anim_arc, 10, LV_PART_INDICATOR);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, _anim_arc);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) { lv_arc_set_angles((lv_obj_t*)obj, v, v + 60); });
    lv_anim_set_values(&a, 0, 360);
    lv_anim_set_duration(&a, 2400);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    _time = lv_label_create(root);
    lv_obj_set_style_text_color(_time, COLOR_FG, 0);
    lv_obj_set_style_text_font(_time, &lv_font_montserrat_48, 0);
    lv_label_set_text(_time, "00:00");
    lv_obj_align(_time, LV_ALIGN_CENTER, 0, -4);

    _date = lv_label_create(root);
    lv_obj_set_style_text_color(_date, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_date, &lv_font_montserrat_14, 0);
    lv_label_set_text(_date, "");
    lv_obj_align(_date, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void AppClock::update_animated(const struct tm& t)
{
    if (!_time) return;
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    lv_label_set_text(_time, buf);
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    lv_label_set_text(_date, buf);
}

/* -------------------------------- seg7 --------------------------------- */

void AppClock::build_seg7(lv_obj_t* root)
{
    lv_obj_set_style_bg_color(root, SEG7_BG, 0);
    if (_bar) {
        lv_obj_set_style_bg_color(_bar, SEG7_BAR_BG, LV_PART_MAIN);
        lv_obj_set_style_bg_color(_bar, SEG7_ON, LV_PART_INDICATOR);
    }
    if (_bar_pct) lv_obj_set_style_text_color(_bar_pct, SEG7_DATE, 0);

    constexpr int CW = 212, CH = 96;
    _buf = new std::uint8_t[CW * CH * 2];
    _canvas = lv_canvas_create(root);
    lv_canvas_set_buffer(_canvas, _buf, CW, CH, LV_COLOR_FORMAT_RGB565);
    lv_obj_align(_canvas, LV_ALIGN_CENTER, 0, -6);

    _date = lv_label_create(root);
    lv_obj_set_style_text_color(_date, SEG7_DATE, 0);
    lv_obj_set_style_text_font(_date, &lv_font_montserrat_14, 0);
    lv_label_set_text(_date, "");
    lv_obj_align(_date, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void AppClock::update_seg7(const struct tm& t)
{
    if (!_canvas) return;
    if (t.tm_sec != _last_sec) {
        _last_sec = t.tm_sec;
        constexpr int CW = 212, CH = 96, DW = 36, DH = 84, T = 7, GAP = 8, COLON_W = 16;

        lv_draw_rect_dsc_t bg;
        lv_draw_rect_dsc_init(&bg);
        bg.bg_color = LV_COLOR_MAKE(0x0A, 0x00, 0x00);
        bg.bg_opa = LV_OPA_COVER;
        lv_layer_t layer;
        lv_canvas_init_layer(_canvas, &layer);
        lv_area_t whole = {0, 0, CW - 1, CH - 1};
        lv_draw_rect(&layer, &bg, &whole);

        int total_w = 4 * DW + COLON_W + 4 * GAP;
        int x = (CW - total_w) / 2;
        int y = (CH - DH) / 2;
        draw_seg7_digit(&layer, x, y, t.tm_hour / 10, DW, DH, T); x += DW + GAP;
        draw_seg7_digit(&layer, x, y, t.tm_hour % 10, DW, DH, T); x += DW + GAP;
        draw_seg7_colon(&layer, x, y, COLON_W, DH, t.tm_sec % 2 == 0); x += COLON_W + GAP;
        draw_seg7_digit(&layer, x, y, t.tm_min / 10, DW, DH, T); x += DW + GAP;
        draw_seg7_digit(&layer, x, y, t.tm_min % 10, DW, DH, T);
        lv_canvas_finish_layer(_canvas, &layer);
    }
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    lv_label_set_text(_date, buf);
}

/* --------------------------------- vfd --------------------------------- */

void AppClock::build_vfd(lv_obj_t* root)
{
    lv_obj_set_style_bg_color(root, VFD_BG, 0);
    if (_bar) {
        lv_obj_set_style_bg_color(_bar, VFD_OFF, LV_PART_MAIN);
        lv_obj_set_style_bg_color(_bar, VFD_ON, LV_PART_INDICATOR);
    }
    if (_bar_pct) lv_obj_set_style_text_color(_bar_pct, VFD_DIM, 0);

    constexpr int CW = 224, CH = 56;
    _buf = new std::uint8_t[CW * CH * 2];
    _canvas = lv_canvas_create(root);
    lv_canvas_set_buffer(_canvas, _buf, CW, CH, LV_COLOR_FORMAT_RGB565);
    lv_obj_align(_canvas, LV_ALIGN_CENTER, 0, -6);

    _date = lv_label_create(root);
    lv_obj_set_style_text_color(_date, VFD_DIM, 0);
    lv_obj_set_style_text_font(_date, &lv_font_montserrat_14, 0);
    lv_label_set_text(_date, "");
    lv_obj_align(_date, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void AppClock::update_vfd(const struct tm& t)
{
    if (!_canvas) return;
    if (t.tm_sec != _last_sec) {
        _last_sec = t.tm_sec;
        constexpr int CW = 224, CH = 56, DOT = 5, PITCH = 7;
        constexpr int GLYPH_W = 5 * PITCH, GLYPH_GAP = 7;
        constexpr int TOTAL_W = 5 * GLYPH_W + 4 * GLYPH_GAP, TOTAL_H = 7 * PITCH;

        lv_draw_rect_dsc_t bg;
        lv_draw_rect_dsc_init(&bg);
        bg.bg_color = VFD_BG;
        bg.bg_opa = LV_OPA_COVER;
        lv_layer_t layer;
        lv_canvas_init_layer(_canvas, &layer);
        lv_area_t whole = {0, 0, CW - 1, CH - 1};
        lv_draw_rect(&layer, &bg, &whole);

        int x = (CW - TOTAL_W) / 2;
        int y = (CH - TOTAL_H) / 2;
        int idxs[5] = {t.tm_hour / 10, t.tm_hour % 10, 10, t.tm_min / 10, t.tm_min % 10};
        for (int i = 0; i < 5; i++) {
            draw_vfd_glyph(&layer, x, y, idxs[i], DOT, PITCH);
            x += GLYPH_W + GLYPH_GAP;
        }
        lv_canvas_finish_layer(_canvas, &layer);
    }
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    lv_label_set_text(_date, buf);
}

/* --------------------------------- flip -------------------------------- */

void AppClock::build_flip(lv_obj_t* root)
{
    auto make_card = [&](int dx) -> lv_obj_t* {
        lv_obj_t* c = lv_obj_create(root);
        lv_obj_remove_style_all(c);
        lv_obj_set_size(c, 92, 112);
        lv_obj_align(c, LV_ALIGN_CENTER, dx, -4);
        lv_obj_set_style_radius(c, 12, 0);
        lv_obj_set_style_bg_color(c, lv_color_hex(0x1d1d2b), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        lv_obj_clear_flag(c, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_t* seam = lv_obj_create(c);
        lv_obj_remove_style_all(seam);
        lv_obj_set_size(seam, 92, 2);
        lv_obj_align(seam, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(seam, COLOR_BG, 0);
        lv_obj_set_style_bg_opa(seam, LV_OPA_COVER, 0);
        return c;
    };
    auto make_digit = [&](lv_obj_t* parent) -> lv_obj_t* {
        lv_obj_t* l = lv_label_create(parent);
        lv_obj_set_style_text_color(l, COLOR_FG, 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_48, 0);
        lv_label_set_text(l, "00");
        lv_obj_center(l);
        return l;
    };
    _hh = make_digit(make_card(-50));
    _mm = make_digit(make_card(50));
    _last_min = -1;

    _date = lv_label_create(root);
    lv_obj_set_style_text_color(_date, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_date, &lv_font_montserrat_14, 0);
    lv_label_set_text(_date, "");
    lv_obj_align(_date, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void AppClock::update_flip(const struct tm& t)
{
    if (!_hh || !_mm) return;
    char hh[12], mm[12];
    std::snprintf(hh, sizeof(hh), "%02d", t.tm_hour);
    std::snprintf(mm, sizeof(mm), "%02d", t.tm_min);
    lv_label_set_text(_hh, hh);
    lv_label_set_text(_mm, mm);

    if (t.tm_min != _last_min) {
        const bool hour_changed = (_last_min >= 0) && (t.tm_min == 0);
        _last_min = t.tm_min;
        auto flap = [](lv_obj_t* o) {
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, o);
            lv_anim_set_values(&a, 0, 256);
            lv_anim_set_duration(&a, 260);
            lv_anim_set_exec_cb(&a, [](void* p, int32_t v) {
                lv_obj_set_style_translate_y((lv_obj_t*)p, -(int)((256 - v) * 12 / 256), 0);
                lv_obj_set_style_opa((lv_obj_t*)p, (lv_opa_t)(80 + v * 175 / 256), 0);
            });
            lv_anim_start(&a);
        };
        flap(_mm);
        if (hour_changed) flap(_hh);
    }

    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    lv_label_set_text(_date, buf);
}

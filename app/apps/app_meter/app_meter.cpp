/**
 * @file app_meter.cpp
 * @brief See app_meter.h.
 */
#include "app_meter.h"

#include "apps/utils/data_service/data_service.h"
#include <cstdio>

using namespace ui;

AppMeter::AppMeter()
{
    setAppInfo().name = "meter";
}

void AppMeter::build(lv_obj_t* root)
{
    _title = lv_label_create(root);
    lv_obj_set_style_text_color(_title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(_title, &lv_font_montserrat_14, 0);
    lv_label_set_text(_title, "CLAUDE METER");
    lv_obj_align(_title, LV_ALIGN_TOP_MID, 0, 12);

    // Concentric rings: outer = 7-day, inner = 5-hour.
    const int outer = 150;
    _d7_arc = make_ring(root, outer, 11);
    lv_obj_align(_d7_arc, LV_ALIGN_CENTER, 0, -14);
    _h5_arc = make_ring(root, outer - 42, 11);
    lv_obj_align(_h5_arc, LV_ALIGN_CENTER, 0, -14);

    _h5_pct = lv_label_create(root);
    lv_obj_set_style_text_font(_h5_pct, &lv_font_montserrat_24, 0);
    lv_label_set_text(_h5_pct, "5H --");
    lv_obj_align(_h5_pct, LV_ALIGN_CENTER, 0, -24);

    _d7_pct = lv_label_create(root);
    lv_obj_set_style_text_font(_d7_pct, &lv_font_montserrat_14, 0);
    lv_label_set_text(_d7_pct, "7D --");
    lv_obj_align(_d7_pct, LV_ALIGN_CENTER, 0, -2);

    _h5_label = lv_label_create(root);
    lv_obj_set_style_text_color(_h5_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_h5_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_h5_label, "5H");
    lv_obj_align(_h5_label, LV_ALIGN_BOTTOM_LEFT, 6, -44);

    _h5_bar = lv_bar_create(root);
    lv_obj_set_size(_h5_bar, SCREEN_W - 56, 8);
    lv_obj_align(_h5_bar, LV_ALIGN_BOTTOM_LEFT, 40, -46);
    lv_bar_set_range(_h5_bar, 0, 100);
    lv_obj_set_style_bg_color(_h5_bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_radius(_h5_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_h5_bar, 2, LV_PART_INDICATOR);

    _d7_label = lv_label_create(root);
    lv_obj_set_style_text_color(_d7_label, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_d7_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(_d7_label, "7D");
    lv_obj_align(_d7_label, LV_ALIGN_BOTTOM_LEFT, 6, -24);

    _d7_bar = lv_bar_create(root);
    lv_obj_set_size(_d7_bar, SCREEN_W - 56, 8);
    lv_obj_align(_d7_bar, LV_ALIGN_BOTTOM_LEFT, 40, -26);
    lv_bar_set_range(_d7_bar, 0, 100);
    lv_obj_set_style_bg_color(_d7_bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_radius(_d7_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_d7_bar, 2, LV_PART_INDICATOR);

    _status = lv_label_create(root);
    lv_obj_set_style_text_color(_status, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_status, &lv_font_montserrat_14, 0);
    lv_label_set_text(_status, "");
    lv_obj_align(_status, LV_ALIGN_BOTTOM_MID, 0, -6);
}

void AppMeter::tick()
{
    if (!_h5_bar) return;
    const appdata::ClaudeData snap = appdata::DataService::instance().claude();

    float p5, p7;
    const char* status_text;
    if (snap.state == appdata::Fetch_OK && snap.pct_five_hour >= 0.0f && snap.pct_seven_day >= 0.0f) {
        p5 = snap.pct_five_hour;
        p7 = snap.pct_seven_day;
        status_text = "live";
    } else {
        _mock5 += 0.7f; if (_mock5 > 100.0f) _mock5 = 0.0f;
        _mock7 += 0.3f; if (_mock7 > 100.0f) _mock7 = 0.0f;
        p5 = _mock5;
        p7 = _mock7;
        status_text = snap.state == appdata::Fetch_Err ? snap.last_err.c_str() : "mock";
    }

    char buf[12];
    const lv_color_t h5c = metric_color(p5, COLOR_5H);
    std::snprintf(buf, sizeof(buf), "%d%%", (int)(p5 + 0.5f));
    lv_label_set_text(_h5_pct, buf);
    lv_obj_set_style_text_color(_h5_pct, h5c, 0);
    lv_bar_set_value(_h5_bar, (int)(p5 + 0.5f), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_h5_bar, h5c, LV_PART_INDICATOR);
    set_ring(_h5_arc, p5, h5c);

    const lv_color_t d7c = metric_color(p7, COLOR_7D);
    std::snprintf(buf, sizeof(buf), "7D %d%%", (int)(p7 + 0.5f));
    lv_label_set_text(_d7_pct, buf);
    lv_obj_set_style_text_color(_d7_pct, d7c, 0);
    lv_bar_set_value(_d7_bar, (int)(p7 + 0.5f), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_d7_bar, d7c, LV_PART_INDICATOR);
    set_ring(_d7_arc, p7, d7c);

    lv_label_set_text(_status, status_text);
}

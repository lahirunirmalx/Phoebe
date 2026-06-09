/**
 * @file app_aqi.cpp
 * @brief See app_aqi.h.
 */
#include "app_aqi.h"

#include "apps/utils/data_service/data_service.h"
#include <cstdio>

using namespace ui;

AppAqi::AppAqi()
{
    setAppInfo().name = "aqi";
}

void AppAqi::build(lv_obj_t* root)
{
    lv_obj_t* title = lv_label_create(root);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "AIR QUALITY");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    _arc = make_ring(root, 150, 12);
    lv_arc_set_range(_arc, 0, 100);
    lv_obj_align(_arc, LV_ALIGN_CENTER, 0, -6);

    _value = lv_label_create(root);
    lv_obj_set_style_text_color(_value, COLOR_FG, 0);
    lv_obj_set_style_text_font(_value, &lv_font_montserrat_48, 0);
    lv_label_set_text(_value, "--");
    lv_obj_align(_value, LV_ALIGN_CENTER, 0, -14);

    _sub = lv_label_create(root);
    lv_obj_set_style_text_color(_sub, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_sub, &lv_font_montserrat_14, 0);
    lv_label_set_text(_sub, "");
    lv_obj_align(_sub, LV_ALIGN_CENTER, 0, 18);
}

void AppAqi::tick()
{
    if (!_value) return;
    const appdata::AqiData a = appdata::DataService::instance().aqi();
    if (!a.ok) {
        lv_label_set_text(_value, "--");
        lv_label_set_text(_sub, a.err.empty() ? "fetching..." : a.err.c_str());
        return;
    }
    const lv_color_t c = aqi_color(a.aqi);
    char b[16];
    std::snprintf(b, sizeof(b), "%d", a.aqi);
    lv_label_set_text(_value, b);
    lv_obj_set_style_text_color(_value, c, 0);
    int v = a.aqi; if (v > 100) v = 100; if (v < 0) v = 0;
    lv_arc_set_value(_arc, v);
    lv_obj_set_style_arc_color(_arc, c, LV_PART_INDICATOR);
    char sub[40];
    std::snprintf(sub, sizeof(sub), "%s  PM2.5 %d", aqi_text(a.aqi), (int)(a.pm25 + 0.5f));
    lv_label_set_text(_sub, sub);
}

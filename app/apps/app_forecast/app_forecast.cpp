/**
 * @file app_forecast.cpp
 * @brief See app_forecast.h.
 */
#include "app_forecast.h"

#include "apps/utils/data_service/data_service.h"
#include <cstdio>

using namespace ui;

namespace {
void style_mini_icon(lv_obj_t* o, int code)
{
    if (!o) return;
    lv_color_t col = COLOR_CLOUD;
    int radius = 6;
    switch (wx_category(code)) {
        case WX_CLEAR: col = COLOR_SUN; radius = 14; break;
        case WX_CLOUD: col = COLOR_CLOUD; radius = 6; break;
        case WX_RAIN:  col = COLOR_RAIN; radius = 6; break;
    }
    lv_obj_set_style_bg_color(o, col, 0);
    lv_obj_set_style_radius(o, radius, 0);
}
} // namespace

AppForecast::AppForecast()
{
    setAppInfo().name = "forecast";
}

void AppForecast::build(lv_obj_t* root)
{
    lv_obj_t* title = lv_label_create(root);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "3-DAY FORECAST");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    for (int i = 0; i < 3; ++i) {
        const int dx = (i - 1) * 74;
        _name[i] = lv_label_create(root);
        lv_obj_set_style_text_color(_name[i], COLOR_FG, 0);
        lv_obj_set_style_text_font(_name[i], &lv_font_montserrat_24, 0);
        lv_label_set_text(_name[i], "");
        lv_obj_align(_name[i], LV_ALIGN_TOP_MID, dx, 52);

        _icon[i] = lv_obj_create(root);
        lv_obj_remove_style_all(_icon[i]);
        lv_obj_set_size(_icon[i], 30, 30);
        lv_obj_set_style_bg_opa(_icon[i], LV_OPA_COVER, 0);
        lv_obj_align(_icon[i], LV_ALIGN_TOP_MID, dx, 96);

        _temp[i] = lv_label_create(root);
        lv_obj_set_style_text_color(_temp[i], COLOR_LABEL_DIM, 0);
        lv_obj_set_style_text_font(_temp[i], &lv_font_montserrat_14, 0);
        lv_label_set_text(_temp[i], "");
        lv_obj_align(_temp[i], LV_ALIGN_TOP_MID, dx, 146);
    }
}

void AppForecast::tick()
{
    if (!_name[0]) return;
    const appdata::ForecastData fc = appdata::DataService::instance().forecast();
    for (int i = 0; i < 3; ++i) {
        if (!fc.ok) {
            lv_label_set_text(_name[i], i == 0 ? (fc.err.empty() ? "..." : fc.err.c_str()) : "");
            lv_label_set_text(_temp[i], "");
            lv_obj_add_flag(_icon[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(_icon[i], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(_name[i], i == 0 ? "Today" : WDAY[fc.d[i].wday % 7]);
        style_mini_icon(_icon[i], fc.d[i].code);
        char b[24];
        std::snprintf(b, sizeof(b), "%d/%d", (int)(fc.d[i].tmax + 0.5f), (int)(fc.d[i].tmin + 0.5f));
        lv_label_set_text(_temp[i], b);
    }
}

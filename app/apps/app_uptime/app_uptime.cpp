/**
 * @file app_uptime.cpp
 * @brief See app_uptime.h.
 */
#include "app_uptime.h"

#include "apps/utils/data_service/data_service.h"
#include <cstdio>

using namespace ui;

AppUptime::AppUptime()
{
    setAppInfo().name = "uptime";
}

void AppUptime::build(lv_obj_t* root)
{
    lv_obj_t* title = lv_label_create(root);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "UPTIME");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    for (int i = 0; i < 5; ++i) {
        const int y = 44 + i * 34;
        _dots[i] = lv_obj_create(root);
        lv_obj_remove_style_all(_dots[i]);
        lv_obj_set_size(_dots[i], 16, 16);
        lv_obj_set_style_radius(_dots[i], 8, 0);
        lv_obj_set_style_bg_color(_dots[i], COLOR_BAR_BG, 0);
        lv_obj_set_style_bg_opa(_dots[i], LV_OPA_COVER, 0);
        lv_obj_align(_dots[i], LV_ALIGN_TOP_LEFT, 14, y);
        lv_obj_add_flag(_dots[i], LV_OBJ_FLAG_HIDDEN);

        _rows[i] = lv_label_create(root);
        lv_obj_set_style_text_color(_rows[i], COLOR_FG, 0);
        lv_obj_set_style_text_font(_rows[i], &lv_font_montserrat_14, 0);
        lv_label_set_text(_rows[i], "");
        lv_obj_align(_rows[i], LV_ALIGN_TOP_LEFT, 40, y);
    }
}

void AppUptime::tick()
{
    if (!_rows[0]) return;
    const appdata::UpData u = appdata::DataService::instance().uptime();
    if (!u.ok) {
        lv_label_set_text(_rows[0], u.err.empty() ? "..." : u.err.c_str());
        lv_obj_add_flag(_dots[0], LV_OBJ_FLAG_HIDDEN);
        for (int i = 1; i < 5; ++i) { lv_label_set_text(_rows[i], ""); lv_obj_add_flag(_dots[i], LV_OBJ_FLAG_HIDDEN); }
        return;
    }
    for (int i = 0; i < 5; ++i) {
        if (i >= u.count) {
            lv_label_set_text(_rows[i], "");
            lv_obj_add_flag(_dots[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(_dots[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(_dots[i], u.sites[i].up ? COLOR_OK : COLOR_DANGER, 0);
        char b[48];
        if (u.sites[i].up) std::snprintf(b, sizeof(b), "%-15s %dms", u.sites[i].host, u.sites[i].ms);
        else std::snprintf(b, sizeof(b), "%s", u.sites[i].host);
        lv_label_set_text(_rows[i], b);
    }
}

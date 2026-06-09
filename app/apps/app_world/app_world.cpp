/**
 * @file app_world.cpp
 * @brief See app_world.h.
 */
#include "app_world.h"

#include <cstdio>
#include <ctime>

using namespace ui;

AppWorld::AppWorld()
{
    setAppInfo().name = "world";
}

void AppWorld::build(lv_obj_t* root)
{
    lv_obj_t* title = lv_label_create(root);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "WORLD CLOCK");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    for (int i = 0; i < 4; ++i) {
        _rows[i] = lv_label_create(root);
        lv_obj_set_style_text_color(_rows[i], COLOR_FG, 0);
        lv_obj_set_style_text_font(_rows[i], &lv_font_montserrat_24, 0);
        lv_label_set_text(_rows[i], "");
        lv_obj_align(_rows[i], LV_ALIGN_TOP_MID, 0, 50 + i * 44);
    }
}

void AppWorld::tick()
{
    if (!_rows[0]) return;

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
        lv_label_set_text(_rows[i], buf);
    }
}

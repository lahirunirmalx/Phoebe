/**
 * @file app_sunmoon.cpp
 * @brief See app_sunmoon.h.
 */
#include "app_sunmoon.h"

#include "apps/utils/data_service/data_service.h"
#include <cmath>
#include <cstdio>
#include <ctime>

using namespace ui;

AppSunMoon::AppSunMoon()
{
    setAppInfo().name = "sunmoon";
}

void AppSunMoon::build(lv_obj_t* root)
{
    lv_obj_t* title = lv_label_create(root);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "SUN & MOON");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    _rise = lv_label_create(root);
    lv_obj_set_style_text_color(_rise, COLOR_SUN, 0);
    lv_obj_set_style_text_font(_rise, &lv_font_montserrat_24, 0);
    lv_label_set_text(_rise, "rise --:--");
    lv_obj_align(_rise, LV_ALIGN_TOP_MID, 0, 42);

    _set = lv_label_create(root);
    lv_obj_set_style_text_color(_set, COLOR_7D, 0);
    lv_obj_set_style_text_font(_set, &lv_font_montserrat_24, 0);
    lv_label_set_text(_set, "set --:--");
    lv_obj_align(_set, LV_ALIGN_TOP_MID, 0, 76);

    _moon_disc = lv_obj_create(root);
    lv_obj_remove_style_all(_moon_disc);
    lv_obj_set_size(_moon_disc, 70, 70);
    lv_obj_set_style_radius(_moon_disc, 35, 0);
    lv_obj_set_style_bg_color(_moon_disc, lv_color_hex(0xEFEFEF), 0);
    lv_obj_set_style_bg_opa(_moon_disc, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(_moon_disc, true, 0);
    lv_obj_align(_moon_disc, LV_ALIGN_CENTER, 0, 28);

    _moon_shadow = lv_obj_create(_moon_disc);
    lv_obj_remove_style_all(_moon_shadow);
    lv_obj_set_size(_moon_shadow, 70, 70);
    lv_obj_set_style_radius(_moon_shadow, 35, 0);
    lv_obj_set_style_bg_color(_moon_shadow, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(_moon_shadow, LV_OPA_COVER, 0);
    lv_obj_align(_moon_shadow, LV_ALIGN_CENTER, 0, 0);

    _moon_name = lv_label_create(root);
    lv_obj_set_style_text_color(_moon_name, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_moon_name, &lv_font_montserrat_14, 0);
    lv_label_set_text(_moon_name, "");
    lv_obj_align(_moon_name, LV_ALIGN_BOTTOM_MID, 0, -8);
}

void AppSunMoon::tick()
{
    if (!_rise) return;
    const appdata::SunData s = appdata::DataService::instance().sun();
    char b[24];
    std::snprintf(b, sizeof(b), "rise %s", s.ok ? s.rise : "--:--"); lv_label_set_text(_rise, b);
    std::snprintf(b, sizeof(b), "set  %s", s.ok ? s.set : "--:--"); lv_label_set_text(_set, b);

    const double SYNODIC = 29.530588853;
    const long ref = 947182440;
    double age = (double)((long)time(nullptr) - ref) / 86400.0;
    age = age - SYNODIC * std::floor(age / SYNODIC);
    const double illum = (1.0 - std::cos(2.0 * M_PI * age / SYNODIC)) / 2.0;
    const bool waxing = age < SYNODIC / 2.0;

    const int dx = (int)((waxing ? -1 : 1) * illum * 140.0);
    lv_obj_align(_moon_shadow, LV_ALIGN_CENTER, dx, 0);

    const char* name = "New Moon";
    if (age < 1.8) name = "New Moon";
    else if (age < 5.5) name = "Waxing Crescent";
    else if (age < 9.2) name = "First Quarter";
    else if (age < 12.9) name = "Waxing Gibbous";
    else if (age < 16.6) name = "Full Moon";
    else if (age < 20.3) name = "Waning Gibbous";
    else if (age < 23.9) name = "Last Quarter";
    else if (age < 27.6) name = "Waning Crescent";
    char mb[40];
    std::snprintf(mb, sizeof(mb), "%s  %d%%", name, (int)(illum * 100 + 0.5));
    lv_label_set_text(_moon_name, mb);
}

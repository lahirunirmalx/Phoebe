/**
 * @file app_weather.cpp
 * @brief See app_weather.h.
 */
#include "app_weather.h"

#include "apps/utils/data_service/data_service.h"
#include <cstdio>

using namespace ui;

AppWeather::AppWeather()
{
    setAppInfo().name = "weather";
}

void AppWeather::build(lv_obj_t* root)
{
    _city = lv_label_create(root);
    lv_obj_set_style_text_color(_city, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(_city, &lv_font_montserrat_14, 0);
    lv_label_set_text(_city, "");
    lv_obj_align(_city, LV_ALIGN_TOP_MID, 0, 8);

    const int icy = 78;

    _cloud = lv_obj_create(root);
    lv_obj_remove_style_all(_cloud);
    lv_obj_set_size(_cloud, 78, 30);
    lv_obj_set_style_radius(_cloud, 15, 0);
    lv_obj_set_style_bg_color(_cloud, COLOR_CLOUD, 0);
    lv_obj_set_style_bg_opa(_cloud, LV_OPA_COVER, 0);
    lv_obj_align(_cloud, LV_ALIGN_TOP_MID, 0, icy);
    auto puff = [&](int dx, int sz) {
        lv_obj_t* p = lv_obj_create(_cloud);
        lv_obj_remove_style_all(p);
        lv_obj_set_size(p, sz, sz);
        lv_obj_set_style_radius(p, sz / 2, 0);
        lv_obj_set_style_bg_color(p, COLOR_CLOUD, 0);
        lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
        lv_obj_align(p, LV_ALIGN_TOP_MID, dx, -sz / 2);
    };
    puff(-16, 26);
    puff(14, 32);

    _sun = lv_obj_create(root);
    lv_obj_remove_style_all(_sun);
    lv_obj_set_size(_sun, 50, 50);
    lv_obj_set_style_radius(_sun, 25, 0);
    lv_obj_set_style_bg_color(_sun, COLOR_SUN, 0);
    lv_obj_set_style_bg_opa(_sun, LV_OPA_COVER, 0);
    lv_obj_align(_sun, LV_ALIGN_TOP_MID, 0, icy + 2);

    for (int i = 0; i < 3; ++i) {
        _drops[i] = lv_obj_create(root);
        lv_obj_remove_style_all(_drops[i]);
        lv_obj_set_size(_drops[i], 4, 12);
        lv_obj_set_style_radius(_drops[i], 2, 0);
        lv_obj_set_style_bg_color(_drops[i], COLOR_RAIN, 0);
        lv_obj_set_style_bg_opa(_drops[i], LV_OPA_COVER, 0);
        lv_obj_align(_drops[i], LV_ALIGN_TOP_MID, (i - 1) * 18, icy + 36);
    }

    _temp = lv_label_create(root);
    lv_obj_set_style_text_color(_temp, COLOR_FG, 0);
    lv_obj_set_style_text_font(_temp, &lv_font_montserrat_48, 0);
    lv_label_set_text(_temp, "--");
    lv_obj_align(_temp, LV_ALIGN_CENTER, 0, 36);

    _cond = lv_label_create(root);
    lv_obj_set_style_text_color(_cond, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_cond, &lv_font_montserrat_14, 0);
    lv_label_set_text(_cond, "");
    lv_obj_align(_cond, LV_ALIGN_CENTER, 0, 74);

    _extra = lv_label_create(root);
    lv_obj_set_style_text_color(_extra, COLOR_LABEL_DIM, 0);
    lv_obj_set_style_text_font(_extra, &lv_font_montserrat_14, 0);
    lv_label_set_text(_extra, "");
    lv_obj_align(_extra, LV_ALIGN_BOTTOM_MID, 0, -8);

    // Sun breathing pulse.
    lv_anim_t sun_anim;
    lv_anim_init(&sun_anim);
    lv_anim_set_var(&sun_anim, _sun);
    lv_anim_set_exec_cb(&sun_anim, [](void* obj, int32_t v) {
        auto* s = static_cast<lv_obj_t*>(obj);
        lv_obj_set_size(s, v, v);
        lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 78 + 2 + (50 - v) / 2);
    });
    lv_anim_set_values(&sun_anim, 46, 56);
    lv_anim_set_duration(&sun_anim, 1100);
    lv_anim_set_playback_duration(&sun_anim, 1100);
    lv_anim_set_repeat_count(&sun_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&sun_anim);

    const int drop_top = icy + 30;
    for (int i = 0; i < 3; ++i) {
        lv_anim_t drop_anim;
        lv_anim_init(&drop_anim);
        lv_anim_set_var(&drop_anim, _drops[i]);
        lv_anim_set_exec_cb(&drop_anim, [](void* obj, int32_t v) { lv_obj_set_y(static_cast<lv_obj_t*>(obj), v); });
        lv_anim_set_values(&drop_anim, drop_top, drop_top + 22);
        lv_anim_set_duration(&drop_anim, 650);
        lv_anim_set_delay(&drop_anim, i * 200);
        lv_anim_set_repeat_count(&drop_anim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&drop_anim);
    }

    _last_code = -2;
    set_icon(-1);
}

void AppWeather::set_icon(int code)
{
    if (!_sun) return;
    const WxCat cat = (code < 0) ? WX_CLOUD : wx_category(code);
    auto show = [](lv_obj_t* o, bool v) {
        if (!o) return;
        if (v) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    };
    show(_sun, cat == WX_CLEAR);
    show(_cloud, cat != WX_CLEAR);
    for (int i = 0; i < 3; ++i) show(_drops[i], cat == WX_RAIN);
}

void AppWeather::tick()
{
    if (!_temp) return;
    lv_label_set_text(_city, HAL::SysCfg().getConfig().weatherCity.c_str());

    const appdata::WeatherData w = appdata::DataService::instance().weather();
    if (!w.ok) {
        lv_label_set_text(_temp, "--");
        lv_label_set_text(_cond, w.err.empty() ? "fetching..." : w.err.c_str());
        lv_label_set_text(_extra, "");
        if (_last_code != -1) { set_icon(-1); _last_code = -1; }
        return;
    }

    char buf[24];
    std::snprintf(buf, sizeof(buf), "%d\xC2\xB0""C", (int)(w.temp_c + 0.5f));
    lv_label_set_text(_temp, buf);
    lv_label_set_text(_cond, wx_text(w.code));

    char extra[40];
    std::snprintf(extra, sizeof(extra), "%d%%  %d km/h",
                  (int)(w.humidity + 0.5f), (int)(w.wind_kmh + 0.5f));
    lv_label_set_text(_extra, extra);

    if (w.code != _last_code) { set_icon(w.code); _last_code = w.code; }
}

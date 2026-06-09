/**
 * @file app_weather.h
 * @brief Current weather: animated sun/cloud/rain icon + temp/condition/stats.
 *        Reads DataService::weather().
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"

class AppWeather : public ui::ScreenApp {
public:
    AppWeather();

protected:
    void build(lv_obj_t* root) override;
    void tick() override;

private:
    void set_icon(int code);

    lv_obj_t* _city = nullptr;
    lv_obj_t* _cloud = nullptr;
    lv_obj_t* _sun = nullptr;
    lv_obj_t* _drops[3] = {nullptr, nullptr, nullptr};
    lv_obj_t* _temp = nullptr;
    lv_obj_t* _cond = nullptr;
    lv_obj_t* _extra = nullptr;
    int _last_code = -2;
};

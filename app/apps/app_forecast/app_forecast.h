/**
 * @file app_forecast.h
 * @brief 3-day forecast. Reads DataService::forecast().
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"

class AppForecast : public ui::ScreenApp {
public:
    AppForecast();

protected:
    void build(lv_obj_t* root) override;
    void tick() override;

private:
    lv_obj_t* _name[3] = {nullptr, nullptr, nullptr};
    lv_obj_t* _icon[3] = {nullptr, nullptr, nullptr};
    lv_obj_t* _temp[3] = {nullptr, nullptr, nullptr};
};

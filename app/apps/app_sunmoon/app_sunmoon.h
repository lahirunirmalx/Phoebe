/**
 * @file app_sunmoon.h
 * @brief Sunrise/sunset + moon phase. Reads DataService::sun().
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"

class AppSunMoon : public ui::ScreenApp {
public:
    AppSunMoon();

protected:
    void build(lv_obj_t* root) override;
    void tick() override;

private:
    lv_obj_t* _rise = nullptr;
    lv_obj_t* _set = nullptr;
    lv_obj_t* _moon_disc = nullptr;
    lv_obj_t* _moon_shadow = nullptr;
    lv_obj_t* _moon_name = nullptr;
};

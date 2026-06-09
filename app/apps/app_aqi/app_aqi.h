/**
 * @file app_aqi.h
 * @brief Air-quality index ring. Reads DataService::aqi().
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"

class AppAqi : public ui::ScreenApp {
public:
    AppAqi();

protected:
    void build(lv_obj_t* root) override;
    void tick() override;

private:
    lv_obj_t* _arc = nullptr;
    lv_obj_t* _value = nullptr;
    lv_obj_t* _sub = nullptr;
};

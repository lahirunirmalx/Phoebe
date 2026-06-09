/**
 * @file app_uptime.h
 * @brief Uptime monitor (up to 5 sites). Reads DataService::uptime().
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"

class AppUptime : public ui::ScreenApp {
public:
    AppUptime();

protected:
    void build(lv_obj_t* root) override;
    void tick() override;

private:
    lv_obj_t* _rows[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* _dots[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
};

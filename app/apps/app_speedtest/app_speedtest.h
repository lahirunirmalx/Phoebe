/**
 * @file app_speedtest.h
 * @brief Internet download speed test (Cloudflare). Triggers a test when opened
 *        and shows the result in Mbps. Reads DataService::speed().
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"

class AppSpeedTest : public ui::ScreenApp {
public:
    AppSpeedTest();

protected:
    void build(lv_obj_t* root) override;
    void tick() override;

private:
    lv_obj_t* _arc = nullptr;
    lv_obj_t* _value = nullptr;
    lv_obj_t* _sub = nullptr;
};

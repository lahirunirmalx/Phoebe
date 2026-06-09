/**
 * @file app_network.h
 * @brief Network ping latency ring. Reads DataService::net().
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"

class AppNetwork : public ui::ScreenApp {
public:
    AppNetwork();

protected:
    void build(lv_obj_t* root) override;
    void tick() override;

private:
    lv_obj_t* _arc = nullptr;
    lv_obj_t* _value = nullptr;
    lv_obj_t* _sub = nullptr;
};

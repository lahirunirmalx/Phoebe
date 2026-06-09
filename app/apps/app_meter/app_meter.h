/**
 * @file app_meter.h
 * @brief Claude usage meter: two concentric rings (5H inner, 7D outer) + bars.
 *        Reads the shared DataService; falls back to drifting mock values.
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"

class AppMeter : public ui::ScreenApp {
public:
    AppMeter();

protected:
    void build(lv_obj_t* root) override;
    void tick() override;

private:
    lv_obj_t* _title = nullptr;
    lv_obj_t* _h5_arc = nullptr;
    lv_obj_t* _d7_arc = nullptr;
    lv_obj_t* _h5_pct = nullptr;
    lv_obj_t* _d7_pct = nullptr;
    lv_obj_t* _h5_label = nullptr;
    lv_obj_t* _h5_bar = nullptr;
    lv_obj_t* _d7_label = nullptr;
    lv_obj_t* _d7_bar = nullptr;
    lv_obj_t* _status = nullptr;

    float _mock5 = 23.0f;
    float _mock7 = 47.0f;
};

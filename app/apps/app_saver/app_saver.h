/**
 * @file app_saver.h
 * @brief Falling starfield screensaver. Self-animating (no data).
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"

class AppSaver : public ui::ScreenApp {
public:
    AppSaver();

protected:
    void build(lv_obj_t* root) override;

private:
    static constexpr int kStarN = 18;
    lv_obj_t* _stars[kStarN] = {};
};

/**
 * @file app_world.h
 * @brief World clock: local + London / New York / Tokyo.
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"

class AppWorld : public ui::ScreenApp {
public:
    AppWorld();

protected:
    void build(lv_obj_t* root) override;
    void tick() override;

private:
    lv_obj_t* _rows[4] = {nullptr, nullptr, nullptr, nullptr};
};

/**
 * @file app_pet.h
 * @brief Bobbing, blinking pet face. Self-animating (no data).
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"

class AppPet : public ui::ScreenApp {
public:
    AppPet();

protected:
    void build(lv_obj_t* root) override;
};

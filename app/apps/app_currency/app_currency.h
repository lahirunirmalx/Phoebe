/**
 * @file app_currency.h
 * @brief FX -> LKR rates. Reads DataService::currency().
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"

class AppCurrency : public ui::ScreenApp {
public:
    AppCurrency();

protected:
    void build(lv_obj_t* root) override;
    void tick() override;

private:
    lv_obj_t* _rows[3] = {nullptr, nullptr, nullptr};
};

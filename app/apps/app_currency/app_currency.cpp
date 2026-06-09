/**
 * @file app_currency.cpp
 * @brief See app_currency.h.
 */
#include "app_currency.h"

#include "apps/utils/data_service/data_service.h"
#include <cstdio>

using namespace ui;

AppCurrency::AppCurrency()
{
    setAppInfo().name = "currency";
}

void AppCurrency::build(lv_obj_t* root)
{
    lv_obj_t* title = lv_label_create(root);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "FX -> LKR");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

    for (int i = 0; i < 3; ++i) {
        _rows[i] = lv_label_create(root);
        lv_obj_set_style_text_color(_rows[i], COLOR_FG, 0);
        lv_obj_set_style_text_font(_rows[i], &lv_font_montserrat_24, 0);
        lv_label_set_text(_rows[i], "");
        lv_obj_align(_rows[i], LV_ALIGN_TOP_MID, 0, 60 + i * 50);
    }
}

void AppCurrency::tick()
{
    if (!_rows[0]) return;
    const appdata::CurrencyData c = appdata::DataService::instance().currency();
    if (!c.ok) {
        lv_label_set_text(_rows[0], c.err.empty() ? "fetching..." : c.err.c_str());
        lv_label_set_text(_rows[1], "");
        lv_label_set_text(_rows[2], "");
        return;
    }
    char b[24];
    std::snprintf(b, sizeof(b), "USD  %.1f", c.usd); lv_label_set_text(_rows[0], b);
    std::snprintf(b, sizeof(b), "EUR  %.1f", c.eur); lv_label_set_text(_rows[1], b);
    std::snprintf(b, sizeof(b), "GBP  %.1f", c.gbp); lv_label_set_text(_rows[2], b);
}

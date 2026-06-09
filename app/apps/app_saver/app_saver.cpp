/**
 * @file app_saver.cpp
 * @brief See app_saver.h.
 */
#include "app_saver.h"

using namespace ui;

AppSaver::AppSaver()
{
    setAppInfo().name = "saver";
}

void AppSaver::build(lv_obj_t* root)
{
    for (int i = 0; i < kStarN; ++i) {
        _stars[i] = lv_obj_create(root);
        lv_obj_remove_style_all(_stars[i]);
        const int sz = 2 + (i % 3);
        lv_obj_set_size(_stars[i], sz, sz + 2);
        lv_obj_set_style_radius(_stars[i], 1, 0);
        lv_obj_set_style_bg_color(_stars[i], (i % 4 == 0) ? COLOR_ACCENT : lv_color_hex(0x66FF99), 0);
        lv_obj_set_style_bg_opa(_stars[i], LV_OPA_COVER, 0);
        lv_obj_set_x(_stars[i], (i * 71 + 13) % (SCREEN_W - 6));

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, _stars[i]);
        lv_anim_set_exec_cb(&a, [](void* o, int32_t v) { lv_obj_set_y((lv_obj_t*)o, v); });
        lv_anim_set_values(&a, -8, SCREEN_H + 8);
        lv_anim_set_duration(&a, 1500 + (i % 6) * 450);
        lv_anim_set_delay(&a, i * 160);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    }
}

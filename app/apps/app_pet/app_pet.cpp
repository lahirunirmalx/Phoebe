/**
 * @file app_pet.cpp
 * @brief See app_pet.h.
 */
#include "app_pet.h"

using namespace ui;

AppPet::AppPet()
{
    setAppInfo().name = "pet";
}

void AppPet::build(lv_obj_t* root)
{
    lv_obj_t* face = lv_obj_create(root);
    lv_obj_remove_style_all(face);
    lv_obj_set_size(face, 130, 120);
    lv_obj_set_style_radius(face, 60, 0);
    lv_obj_set_style_bg_color(face, COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(face, LV_OPA_COVER, 0);
    lv_obj_align(face, LV_ALIGN_CENTER, 0, 0);

    auto eye = [&](int dx) {
        lv_obj_t* e = lv_obj_create(face);
        lv_obj_remove_style_all(e);
        lv_obj_set_size(e, 18, 18);
        lv_obj_set_style_radius(e, 9, 0);
        lv_obj_set_style_bg_color(e, lv_color_hex(0x101010), 0);
        lv_obj_set_style_bg_opa(e, LV_OPA_COVER, 0);
        lv_obj_align(e, LV_ALIGN_CENTER, dx, -14);
        return e;
    };
    lv_obj_t* eyeL = eye(-26);
    lv_obj_t* eyeR = eye(26);

    lv_obj_t* mouth = lv_obj_create(face);
    lv_obj_remove_style_all(mouth);
    lv_obj_set_size(mouth, 44, 10);
    lv_obj_set_style_radius(mouth, 5, 0);
    lv_obj_set_style_bg_color(mouth, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(mouth, LV_OPA_COVER, 0);
    lv_obj_align(mouth, LV_ALIGN_CENTER, 0, 26);

    lv_anim_t bob;
    lv_anim_init(&bob);
    lv_anim_set_var(&bob, face);
    lv_anim_set_exec_cb(&bob, [](void* o, int32_t v) { lv_obj_align((lv_obj_t*)o, LV_ALIGN_CENTER, 0, v); });
    lv_anim_set_values(&bob, -14, 14);
    lv_anim_set_duration(&bob, 900);
    lv_anim_set_playback_duration(&bob, 900);
    lv_anim_set_repeat_count(&bob, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&bob);

    auto blink = [](lv_obj_t* e) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, e);
        lv_anim_set_exec_cb(&a, [](void* o, int32_t v) { lv_obj_set_height((lv_obj_t*)o, v); });
        lv_anim_set_values(&a, 18, 2);
        lv_anim_set_duration(&a, 120);
        lv_anim_set_playback_duration(&a, 120);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_repeat_delay(&a, 2600);
        lv_anim_start(&a);
    };
    blink(eyeL);
    blink(eyeR);
}

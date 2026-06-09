/**
 * @file app_matrix.cpp
 * @brief See app_matrix.h.
 */
#include "app_matrix.h"

using namespace ui;

namespace {
void fill_column(char* out, int rows, std::uint32_t& rng)
{
    static const char cs[] = "0123456789ABCDEFGHJKLMNPQRSTUVWXYZ#$%&*+=<>?";
    const int n = (int)sizeof(cs) - 1;
    int p = 0;
    for (int r = 0; r < rows; ++r) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        out[p++] = cs[rng % n];
        if (r < rows - 1) out[p++] = '\n';
    }
    out[p] = '\0';
}
} // namespace

AppMatrix::AppMatrix()
{
    setAppInfo().name = "matrix";
}

void AppMatrix::build(lv_obj_t* root)
{
    lv_obj_set_style_bg_color(root, lv_color_hex(0x001005), 0); // dark green base

    const int colw = SCREEN_W / kCols;
    char buf[kRows * 2 + 1];
    for (int i = 0; i < kCols; ++i) {
        lv_obj_t* l = lv_label_create(root);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(l, lv_color_hex((i % 5 == 0) ? 0x99ff99 : 0x33dd55), 0);
        lv_obj_set_style_text_line_space(l, 3, 0);
        fill_column(buf, kRows, _rng);
        lv_label_set_text(l, buf);
        lv_obj_set_x(l, i * colw + 4);
        _cols[i] = l;

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, l);
        lv_anim_set_exec_cb(&a, [](void* o, int32_t v) { lv_obj_set_y((lv_obj_t*)o, v); });
        lv_anim_set_values(&a, -360, SCREEN_H);
        lv_anim_set_duration(&a, 2200 + (i % 6) * 480);
        lv_anim_set_delay(&a, i * 130);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    }
}

void AppMatrix::tick()
{
    char buf[kRows * 2 + 1];
    for (int i = 0; i < kCols; ++i) {
        if (!_cols[i]) continue;
        fill_column(buf, kRows, _rng);
        lv_label_set_text(_cols[i], buf);
    }
}

/**
 * @file mouse_cursor_icon.c
 * @brief 16x16 ARGB8888 white arrow cursor used by the SDL desktop simulator.
 *        Referenced via LV_IMAGE_DECLARE(mouse_cursor_icon) in hal_desktop.cpp.
 */
#include <lvgl.h>

#define _ 0x00, 0x00, 0x00, 0x00,
#define X 0xFF, 0xFF, 0xFF, 0xFF,

static const uint8_t mouse_cursor_icon_map[] = {
    X _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
    X X _ _ _ _ _ _ _ _ _ _ _ _ _ _
    X X X _ _ _ _ _ _ _ _ _ _ _ _ _
    X X X X _ _ _ _ _ _ _ _ _ _ _ _
    X X X X X _ _ _ _ _ _ _ _ _ _ _
    X X X X X X _ _ _ _ _ _ _ _ _ _
    X X X X X X X _ _ _ _ _ _ _ _ _
    X X X X X X X X _ _ _ _ _ _ _ _
    X X X X X X X X X _ _ _ _ _ _ _
    X X X X X X X X X X _ _ _ _ _ _
    X X X X X X _ _ _ _ _ _ _ _ _ _
    X X X _ X X X _ _ _ _ _ _ _ _ _
    X X _ _ X X X _ _ _ _ _ _ _ _ _
    X _ _ _ _ X X X _ _ _ _ _ _ _ _
    _ _ _ _ _ X X X _ _ _ _ _ _ _ _
    _ _ _ _ _ _ X _ _ _ _ _ _ _ _ _
};

#undef _
#undef X

const lv_image_dsc_t mouse_cursor_icon = {
    .header.w = 16,
    .header.h = 16,
    .header.stride = 16 * 4,
    .header.cf = LV_COLOR_FORMAT_ARGB8888,
    .data = mouse_cursor_icon_map,
    .data_size = sizeof(mouse_cursor_icon_map),
};

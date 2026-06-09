/**
 * @file app_life.cpp
 * @brief See app_life.h.
 */
#include "app_life.h"

using namespace ui;

AppLife::AppLife()
{
    setAppInfo().name = "life";
}

void AppLife::seed()
{
    _rng ^= (HAL::SysCtrl().millis() * 2654435761u) | 1u;
    auto rnd = [this]() {
        std::uint32_t x = _rng;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        _rng = x;
        return x;
    };
    for (int i = 0; i < G * G; ++i)
        _cur[i] = (rnd() % 100) < 28 ? 1 : 0;
    _gen = 0;
    _static = 0;
    _prev_pop = -1;
}

void AppLife::build(lv_obj_t* root)
{
    _buf = new std::uint8_t[G * G * 2];
    _canvas = lv_canvas_create(root);
    lv_canvas_set_buffer(_canvas, _buf, G, G, LV_COLOR_FORMAT_RGB565);
    lv_obj_align(_canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_image_set_pivot(_canvas, 0, 0);
    lv_image_set_antialias(_canvas, false);
    lv_image_set_scale(_canvas, 256 * (SCREEN_W / G)); // crisp nearest-neighbour upscale
    lv_canvas_fill_bg(_canvas, COLOR_BG, LV_OPA_COVER);

    _cur.assign(G * G, 0);
    _next.assign(G * G, 0);
    seed();
}

void AppLife::tick()
{
    if (!_canvas || _cur.empty()) return;

    int pop = 0;
    for (int y = 0; y < G; ++y) {
        const int yu = (y + G - 1) % G;
        const int yd = (y + 1) % G;
        for (int x = 0; x < G; ++x) {
            const int xl = (x + G - 1) % G;
            const int xr = (x + 1) % G;
            const int n = _cur[yu * G + xl] + _cur[yu * G + x] + _cur[yu * G + xr]
                        + _cur[y  * G + xl]                    + _cur[y  * G + xr]
                        + _cur[yd * G + xl] + _cur[yd * G + x] + _cur[yd * G + xr];
            const std::uint8_t alive = _cur[y * G + x];
            const std::uint8_t next = (n == 3 || (alive && n == 2)) ? 1 : 0;
            _next[y * G + x] = next;
            pop += next;
        }
    }
    _cur.swap(_next);

    if (pop == _prev_pop) _static++;
    else _static = 0;
    _prev_pop = pop;
    if (pop == 0 || _static > 14 || ++_gen > 600) seed();

    lv_canvas_fill_bg(_canvas, COLOR_BG, LV_OPA_COVER);
    for (int y = 0; y < G; ++y)
        for (int x = 0; x < G; ++x)
            if (_cur[y * G + x])
                lv_canvas_set_px(_canvas, x, y, COLOR_ACCENT, LV_OPA_COVER);
}

void AppLife::teardown()
{
    _canvas = nullptr; // deleted with root
    delete[] _buf;
    _buf = nullptr;
}

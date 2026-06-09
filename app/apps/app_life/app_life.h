/**
 * @file app_life.h
 * @brief Conway's Game of Life, full-screen (tiny canvas scaled up). No data.
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"
#include <cstdint>
#include <vector>

class AppLife : public ui::ScreenApp {
public:
    AppLife();

protected:
    void build(lv_obj_t* root) override;
    void tick() override;
    void teardown() override;

private:
    void seed();

    static constexpr int G = 60; // cells per side (G * scale = SCREEN_W)
    lv_obj_t* _canvas = nullptr;
    std::uint8_t* _buf = nullptr;
    std::vector<std::uint8_t> _cur, _next;
    std::uint32_t _rng = 0x1234567u;
    int _gen = 0;
    int _static = 0;
    int _prev_pop = -1;
};

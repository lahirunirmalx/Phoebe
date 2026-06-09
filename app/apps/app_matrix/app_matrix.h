/**
 * @file app_matrix.h
 * @brief Matrix rain: falling green character columns. No data.
 */
#pragma once

#include "apps/utils/ui_common/screen_app.h"
#include <cstdint>

class AppMatrix : public ui::ScreenApp {
public:
    AppMatrix();

protected:
    void build(lv_obj_t* root) override;
    void tick() override;

private:
    static constexpr int kCols = 16;
    static constexpr int kRows = 17;
    lv_obj_t* _cols[kCols] = {};
    std::uint32_t _rng = 0x9e3779b9u;
};

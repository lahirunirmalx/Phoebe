/**
 * @file system_ctrl_sdl.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <hal/hal.h>

// SDL implementation
class SystemControlSdl : public hal_components::SystemControlBase {
public:
    std::uint32_t millis() override;
    void delay(std::uint32_t ms) override;
};

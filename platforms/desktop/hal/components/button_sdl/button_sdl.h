/**
 * @file button_sdl.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-13
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <hal/hal.h>

// SDL implementation
class ButtonSdl : public hal_components::ButtonBase {
public:
    bool getButton(ButtonId::ButtonId_t id) override;
    std::uint32_t millis() override;
};

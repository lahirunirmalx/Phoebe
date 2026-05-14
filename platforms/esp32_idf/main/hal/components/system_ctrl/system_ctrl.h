/**
 * @file system_ctrl.h
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

// Arduino implementation
class SystemControlArduino : public hal_components::SystemControlBase {
public:
    void init() override;
    std::uint32_t millis() override;
    void delay(std::uint32_t ms) override;
    void reboot() override;
    void powerOff() override;
    void feedTheDog() override;
    size_t freeHeapSize() override;
};

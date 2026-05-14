/**
 * @file button.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-13
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include "utils/Button_Class/Button_Class.hpp"

namespace ButtonId {
enum ButtonId_t {
    None = 0,
    Power,
    Up,
    Ok,
    Down,
};
}

namespace hal_components {

/**
 * @brief Button component base class
 *
 */
class ButtonBase {
public:
    virtual void init() {}

    /**
     * @brief Lowest-level button state read, e.g. raw pin level
     *
     * @param id
     * @return true Pressed
     * @return false Not pressed
     */
    virtual bool getButton(ButtonId::ButtonId_t id)
    {
        return false;
    }

    /**
     * @brief Get the current system uptime in milliseconds, used for debounce, double-click, etc.
     *
     * @return std::uint32_t
     */
    virtual std::uint32_t millis()
    {
        return 0;
    }

    // Button state class instances, one per button ID
    Button_Class BtnPower;
    Button_Class BtnUp;
    Button_Class BtnOk;
    Button_Class BtnDown;

    /**
     * @brief Refresh button states
     *
     */
    inline void update()
    {
        BtnPower.setRawState(millis(), getButton(ButtonId::Power));
        BtnUp.setRawState(millis(), getButton(ButtonId::Up));
        BtnOk.setRawState(millis(), getButton(ButtonId::Ok));
        BtnDown.setRawState(millis(), getButton(ButtonId::Down));
    }
};

} // namespace hal_components

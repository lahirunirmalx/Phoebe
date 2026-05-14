/**
 * @file battery_monitor.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-13
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <functional>

namespace BatteryState {
enum BatteryState_t {
    NotConnected = 0, // Not connected
    Charging,         // Charging
    Normal,           // Normal discharge
    Low,              // Low battery
    Dead,             // Dead
};
}

namespace hal_components {

/**
 * @brief Battery monitor base class
 *
 */
class BatteryMonitorBase {
public:
    virtual bool init()
    {
        return false;
    }

    virtual float voltage()
    {
        return 0.0f;
    }

    virtual float percent()
    {
        return 0.0f;
    }

    virtual BatteryState::BatteryState_t state()
    {
        return BatteryState::NotConnected;
    }

    virtual void onChargingStart(std::function<void()> callback) {}

    virtual void onChargingStop(std::function<void()> callback) {}

    virtual void onBatteryLow(std::function<void()> callback) {}

    virtual void onBatterDead(std::function<void()> callback) {}
};

} // namespace hal_components

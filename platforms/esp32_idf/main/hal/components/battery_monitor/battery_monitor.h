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
#include <hal/hal.h>

// MAX17048 implementation
class BatteryMonitorMAX17048 : public hal_components::BatteryMonitorBase {
public:
    BatteryMonitorMAX17048();
    ~BatteryMonitorMAX17048();

    bool init() override;
    float voltage() override;
    float percent() override;
    BatteryState::BatteryState_t state() override;
    void onChargingStart(std::function<void()> callback) override;
    void onChargingStop(std::function<void()> callback) override;
    void onBatteryLow(std::function<void()> callback) override;
    void onBatterDead(std::function<void()> callback) override;
};

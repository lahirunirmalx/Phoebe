/**
 * @file hal.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-09-29
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <memory>
#include <string>
#include "components/system_control.h"
#include "components/imu.h"
#include "components/buzzer.h"
#include "components/system_config.h"
#include "components/display.h"
#include "components/haptic_engine.h"
#include "components/battery_monitor.h"
#include "components/button.h"
#include "components/ble.h"
#include "components/wifi_manager.h"
#include "components/http_client.h"
#include "components/backlight.h"

/**
 * @brief Hardware abstraction layer, providing unified hardware/platform-specific behavior interfaces
 *
 */
namespace HAL {

/**
 * @brief Hardware abstraction base class
 *
 */
class HalBase {
public:
    virtual ~HalBase() = default;

    /* -------------------------------------------------------------------------- */
    /*                                Hardware APIs                               */
    /* -------------------------------------------------------------------------- */
    // Hardware behavior abstractions, i.e. methods to override when creating your own HAL
    // Add the virtual methods you need here
    // For example, fetching info from an HTTP endpoint:
    // virtual std::string fetchInfoFromHttp(std::string api) { return ""; }
    // For more complex behaviors, encapsulate them as components; see the components directory

    /**
     * @brief Get the HAL type
     *
     * @return std::string
     */
    virtual std::string type()
    {
        return "Base";
    }

    /**
     * @brief Initialize hardware and create component instances here
     *
     */
    virtual void init() {}

    /* -------------------------------------------------------------------------- */
    /*                              Components Getter                             */
    /* -------------------------------------------------------------------------- */
    // Component instance accessors
    hal_components::SystemControlBase& SysCtrl();
    hal_components::ImuBase& Imu();
    hal_components::BuzzerBase& Buzzer();
    hal_components::SystemConfigBase& SysCfg();
    hal_components::DisplayBase& Display();
    hal_components::HapticEngineBase& HapticEngine();
    hal_components::BatteryMonitorBase& BatteryMonitor();
    hal_components::ButtonBase& Button();
    hal_components::BleBase& Ble();
    hal_components::WifiManagerBase& Wifi();
    hal_components::HttpClientBase& Http();
    hal_components::BacklightBase& Backlight();

protected:
    // Component instance management
    struct Components_t {
        std::unique_ptr<hal_components::SystemControlBase> system_control;
        std::unique_ptr<hal_components::ImuBase> imu;
        std::unique_ptr<hal_components::BuzzerBase> buzzer;
        std::unique_ptr<hal_components::SystemConfigBase> system_config;
        std::unique_ptr<hal_components::DisplayBase> display;
        std::unique_ptr<hal_components::HapticEngineBase> haptic_engine;
        std::unique_ptr<hal_components::BatteryMonitorBase> battery_monitor;
        std::unique_ptr<hal_components::ButtonBase> button;
        std::unique_ptr<hal_components::BleBase> ble;
        std::unique_ptr<hal_components::WifiManagerBase> wifi;
        std::unique_ptr<hal_components::HttpClientBase> http_client;
        std::unique_ptr<hal_components::BacklightBase> backlight;
    };
    Components_t _components;
};

/* -------------------------------------------------------------------------- */
/*                                  Singleton                                 */
/* -------------------------------------------------------------------------- */
// Provides an injectable global singleton

/**
 * @brief Get the current HAL instance
 *
 * @return HalBase&
 */
HalBase& Get();

/**
 * @brief Inject a HAL; init() will be called to initialize it
 *
 * @param hal
 */
void Inject(std::unique_ptr<HalBase> hal);

/**
 * @brief Destroy the current HAL instance
 *
 */
void Destroy();

// Convenience wrappers to keep call sites short
inline hal_components::SystemControlBase& SysCtrl()
{
    return Get().SysCtrl();
}
inline hal_components::ImuBase& Imu()
{
    return Get().Imu();
}
inline hal_components::BuzzerBase& Buzzer()
{
    return Get().Buzzer();
}
inline hal_components::SystemConfigBase& SysCfg()
{
    return Get().SysCfg();
}
inline hal_components::DisplayBase& Display()
{
    return Get().Display();
}
inline hal_components::HapticEngineBase& HapticEngine()
{
    return Get().HapticEngine();
}
inline hal_components::BatteryMonitorBase& BatteryMonitor()
{
    return Get().BatteryMonitor();
}
inline void BtnUpdate()
{
    Get().Button().update();
}
inline hal_components::Button_Class& BtnPower()
{
    return Get().Button().BtnPower;
}
inline hal_components::Button_Class& BtnUp()
{
    return Get().Button().BtnUp;
}
inline hal_components::Button_Class& BtnOk()
{
    return Get().Button().BtnOk;
}
inline hal_components::Button_Class& BtnDown()
{
    return Get().Button().BtnDown;
}
inline hal_components::BleBase& Ble()
{
    return Get().Ble();
}
inline hal_components::WifiManagerBase& Wifi()
{
    return Get().Wifi();
}
inline hal_components::HttpClientBase& Http()
{
    return Get().Http();
}
inline hal_components::BacklightBase& Backlight()
{
    return Get().Backlight();
}

} // namespace HAL

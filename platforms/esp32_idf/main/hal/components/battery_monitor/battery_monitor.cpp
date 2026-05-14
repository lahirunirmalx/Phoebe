/**
 * @file battery_monitor.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-13
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "battery_monitor.h"
#include "../../hal_config.h"
#include "../utils/Adafruit_MAX1704X/Adafruit_MAX1704X.h"
#include "hal/components/battery_monitor.h"
#include <Arduino.h>
#include <mooncake_log.h>
#include <mutex>

static const char* _tag = "BatMon";

struct _Data_t {
    std::mutex mutex;
    Adafruit_MAX17048 max17048;
    float bat_voltage = 0.0f;
    float bat_percentage = 0.0f;
    BatteryState::BatteryState_t bat_state = BatteryState::NotConnected;
    std::function<void()> on_charging_start = nullptr;
    std::function<void()> on_charging_stop = nullptr;
    std::function<void()> on_battery_low = nullptr;
    std::function<void()> on_battery_dead = nullptr;
};
static _Data_t* _data = nullptr;

// Monitor thread
static void _daemon_battery_monitor(void* param)
{
    while (1) {
        _data->mutex.lock();

        // Data
        _data->bat_voltage = _data->max17048.cellVoltage();
        _data->bat_percentage = _data->max17048.cellPercent();

        // Charging state
        if (digitalRead(HAL_PIN_PWR_IS_USB_IN) == 1) {
            if (_data->bat_state != BatteryState::Charging) {
                mclog::tagInfo(_tag, "start charging");
                _data->bat_state = BatteryState::Charging;
                if (_data->on_charging_start) {
                    _data->on_charging_start();
                }
            }
        } else if (_data->bat_state == BatteryState::Charging) {
            mclog::tagInfo(_tag, "stop charging");
            _data->bat_state = BatteryState::Normal;
            if (_data->on_charging_stop) {
                _data->on_charging_stop();
            }
        }

        // Low battery detection
        if (_data->bat_state == BatteryState::Normal) {
            // Already dead at startup
            if (round(_data->bat_percentage) < HAL_BATTERY_DEAD_THRESHOLD) {
                _data->bat_state = BatteryState::Dead;
                if (_data->on_battery_dead) {
                    _data->on_battery_dead();
                }

            }
            // Low at startup, or normally discharged down to low
            else if (round(_data->bat_percentage) < HAL_BATTERY_LOW_THRESHOLD) {
                _data->bat_state = BatteryState::Low;
                if (_data->on_battery_low) {
                    _data->on_battery_low();
                }
            }
        }
        // Discharged from low to dead
        else if (_data->bat_state == BatteryState::Low) {
            if (round(_data->bat_percentage) < HAL_BATTERY_DEAD_THRESHOLD) {
                _data->bat_state = BatteryState::Dead;
                if (_data->on_battery_dead) {
                    _data->on_battery_dead();
                }
            }
        }

        _data->mutex.unlock();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}

BatteryMonitorMAX17048::BatteryMonitorMAX17048()
{
    _data = new _Data_t;
}

BatteryMonitorMAX17048::~BatteryMonitorMAX17048()
{
    // Should stop the thread first; not bothering to write that
    delete _data;
}

bool BatteryMonitorMAX17048::init()
{
    if (!_data->max17048.begin(HAL_I2C_BUS_PORT_NUM)) {
        _data->bat_state = BatteryState::NotConnected;
        mclog::tagError(_tag, "init failed");
        return false;
    }
    _data->bat_state = BatteryState::Normal;
    mclog::tagInfo(_tag, "chip id: 0x{:02X}", _data->max17048.getChipID());

    // USB-in detection pin
    pinMode(HAL_PIN_PWR_IS_USB_IN, INPUT);

    // Interrupt pin, not bothering to use it
    gpio_reset_pin((gpio_num_t)HAL_PIN_BAT_MON_INT);
    gpio_set_direction((gpio_num_t)HAL_PIN_BAT_MON_INT, GPIO_MODE_DISABLE);

    // Create monitor thread
    xTaskCreate(_daemon_battery_monitor, "bm", 3000, NULL, 10, NULL);

    return true;
}

float BatteryMonitorMAX17048::voltage()
{
    _data->mutex.lock();
    auto ret = _data->bat_voltage;
    _data->mutex.unlock();
    return ret;
}

float BatteryMonitorMAX17048::percent()
{
    _data->mutex.lock();
    auto ret = _data->bat_percentage;
    _data->mutex.unlock();
    return ret;
}

BatteryState::BatteryState_t BatteryMonitorMAX17048::state()
{
    _data->mutex.lock();
    auto ret = _data->bat_state;
    _data->mutex.unlock();
    return ret;
}

void BatteryMonitorMAX17048::onChargingStart(std::function<void()> callback)
{
    _data->mutex.lock();
    _data->on_charging_start = callback;
    _data->mutex.unlock();
}

void BatteryMonitorMAX17048::onChargingStop(std::function<void()> callback)
{
    _data->mutex.lock();
    _data->on_charging_stop = callback;
    _data->mutex.unlock();
}

void BatteryMonitorMAX17048::onBatteryLow(std::function<void()> callback)
{
    _data->mutex.lock();
    _data->on_battery_low = callback;
    _data->mutex.unlock();
}

void BatteryMonitorMAX17048::onBatterDead(std::function<void()> callback)
{
    _data->mutex.lock();
    _data->on_battery_dead = callback;
    _data->mutex.unlock();
}

/**
 * @file wifi_manager_esp32.h
 * @brief ESP32 wifi manager: credentials in NVS, Arduino WiFi drives the
 *        radio (matches M5Cardputer-UserDemo's HTTPClient stack).
 *        NVS layout: namespace "wifi", keys "ssid" / "password".
 */
#pragma once
#include "hal/components/wifi_manager.h"
#include <string>

class WifiManagerEsp32 : public hal_components::WifiManagerBase {
public:
    WifiManagerEsp32() = default;

    bool load() override;
    bool save() override;
    void setCredentials(const std::string& ssid, const std::string& password) override;
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    void logState() const override;

private:
    // Retained for the legacy event-handler hook; kept static so a future
    // event-driven impl can flip it without invasive changes.
    static volatile bool _s_connected;
    bool _wifi_initialised = false;

    void _ensure_init();
};

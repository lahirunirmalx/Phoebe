/**
 * @file wifi_manager_std.h
 * @brief Desktop WiFi manager: credentials live in SystemConfig
 *        (system_config.json). connect() is a simulation -- there is no
 *        real radio on the host.
 */
#pragma once
#include "hal/components/wifi_manager.h"

class WifiManagerStd : public hal_components::WifiManagerBase {
public:
    WifiManagerStd() = default;

    bool load() override;
    bool save() override;
    void setCredentials(const std::string& ssid, const std::string& password) override;
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    void logState() const override;

private:
    bool _connected = false;
};

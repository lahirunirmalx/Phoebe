/**
 * @file wifi_manager_std.h
 * @brief Desktop WiFi manager: stores SSID/password in a JSON file.
 *        connect() is a simulation -- there is no real radio on the host.
 */
#pragma once
#include "hal/components/wifi_manager.h"
#include <string>

class WifiManagerStd : public hal_components::WifiManagerBase {
public:
    explicit WifiManagerStd(const std::string& rootPath = "./");

    bool load() override;
    bool save() override;
    void setCredentials(const std::string& ssid, const std::string& password) override;
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    void logState() const override;

private:
    std::string _wifi_config_path;
    bool _connected = false;

    void _maybe_seed_from_env();
};

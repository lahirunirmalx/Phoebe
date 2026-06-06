/**
 * @file system_config_esp32.h
 * @brief NVS-backed SystemConfig for the Freenove Mini TV.
 *
 * Persists the full settings struct across three NVS namespaces:
 *   - "wifi"   : ssid, password          (shared with WifiManagerEsp32)
 *   - "claude" : base, bearer
 *   - "phoebe" : mute, hapticFeedback, watchFace, widgetA, widgetB
 *
 * The captive portal writes the form into setConfig() then calls saveConfig();
 * loadConfig() runs at boot. Keeping the "wifi" namespace/keys identical to
 * WifiManagerEsp32 means that manager stays the source of truth for the radio.
 */
#pragma once
#include <hal/hal.h>

class SystemConfigEsp32 : public hal_components::SystemConfigBase {
public:
    bool loadConfig() override;
    bool saveConfig() override;
    bool applyConfig() override;   // applies the timezone (TZ env) -- call after wifi/SNTP
    void logConfig() override;
};

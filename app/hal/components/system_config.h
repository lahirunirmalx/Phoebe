/**
 * @file system_config.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-09-30
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <string>

namespace hal_components {

/**
 * @brief System config component base class
 *
 */
class SystemConfigBase {
public:
    struct Config_t {
        // Configuration values to persist
        bool mute = false;
        bool hapticFeedback = true;
        std::string watchFace;
        std::string widgetA = "time";
        std::string widgetB = "date";

        // Timezone offset from UTC, in minutes (e.g. +330 = UTC+5:30). Applied
        // via the TZ env var on ESP32 so localtime() is correct.
        int tzOffsetMin = 0;

        // Claude usage endpoint (consumed by AppClaudeMeter).
        // base: e.g. "http://127.0.0.1:7878"
        // bearer: opaque API token sent as "Authorization: Bearer <token>"
        std::string claudeBase;
        std::string claudeBearer;

        // WiFi credentials. Desktop persists these inside the same JSON;
        // ESP32 keeps them in NVS namespace "wifi" (NVS-backed
        // WifiManagerEsp32 is the source of truth there).
        std::string wifiSsid;
        std::string wifiPassword;
    };

    bool isClaudeReady() const
    {
        return !_config.claudeBase.empty() && !_config.claudeBearer.empty();
    }

    bool isWifiReady() const
    {
        return !_config.wifiSsid.empty();
    }

    ~SystemConfigBase() = default;

    /**
     * @brief Load config from the file system
     *
     * @return true
     * @return false
     */
    virtual bool loadConfig()
    {
        return false;
    }

    /**
     * @brief Save config to the file system
     *
     * @return true
     * @return false
     */
    virtual bool saveConfig()
    {
        return false;
    }

    /**
     * @brief Get the current system config
     *
     * @return const Config_t&
     */
    virtual const Config_t& getConfig()
    {
        return _config;
    }

    /**
     * @brief Set the current system config
     *
     * @return Config_t&
     */
    virtual Config_t& setConfig()
    {
        return _config;
    }

    /**
     * @brief Apply the current system config
     *
     * @return true
     * @return false
     */
    virtual bool applyConfig()
    {
        return false;
    }

    /**
     * @brief Log the config for inspection
     *
     */
    virtual void logConfig() {}

protected:
    Config_t _config;
};

} // namespace hal_components

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
    };

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

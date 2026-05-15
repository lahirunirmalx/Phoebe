/**
 * @file wifi_manager_std.cpp
 * @brief Desktop wifi manager backed by SystemConfig (system_config.json).
 *        Reads credentials at load() time; writes them back via setConfig
 *        + saveConfig on setCredentials().
 */
#include "wifi_manager_std.h"
#include "hal/hal.h"
#include <mooncake_log.h>

static const char* _tag = "wifi";

bool WifiManagerStd::load()
{
    const auto& cfg = HAL::SysCfg().getConfig();
    _ssid = cfg.wifiSsid;
    _password = cfg.wifiPassword;
    mclog::tagInfo(_tag, "loaded ssid: {}", _ssid.empty() ? "<empty>" : _ssid);
    return hasCredentials();
}

bool WifiManagerStd::save()
{
    auto& cfg = HAL::SysCfg().setConfig();
    cfg.wifiSsid = _ssid;
    cfg.wifiPassword = _password;
    return HAL::SysCfg().saveConfig();
}

void WifiManagerStd::setCredentials(const std::string& ssid, const std::string& password)
{
    WifiManagerBase::setCredentials(ssid, password);
    save();
    _connected = false;
}

bool WifiManagerStd::connect()
{
    if (!hasCredentials()) {
        mclog::tagWarn(_tag, "no credentials, cannot connect");
        _connected = false;
        return false;
    }
    // Desktop sim: pretend the link is up.
    mclog::tagInfo(_tag, "connect (sim) ssid={}", _ssid);
    _connected = true;
    return true;
}

void WifiManagerStd::disconnect()
{
    mclog::tagInfo(_tag, "disconnect (sim)");
    _connected = false;
}

bool WifiManagerStd::isConnected() const
{
    return _connected;
}

void WifiManagerStd::logState() const
{
    mclog::tagInfo(_tag, "ssid: {}  has_password: {}  connected: {}",
                   _ssid.empty() ? "<unset>" : _ssid, !_password.empty(), _connected);
}

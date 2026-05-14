/**
 * @file wifi_manager_std.cpp
 * @brief Desktop wifi manager backed by a JSON file. On first run, seeds
 *        credentials from the env vars PHOEBE_WIFI_SSID / PHOEBE_WIFI_PASSWORD
 *        if the file doesn't exist yet.
 */
#include "wifi_manager_std.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <cstdlib>
#include <mooncake_log.h>
#include <string>

static const char* _tag = "wifi";

WifiManagerStd::WifiManagerStd(const std::string& rootPath)
{
    _wifi_config_path = rootPath + "wifi_config.json";
}

bool WifiManagerStd::load()
{
    mclog::tagInfo(_tag, "load credentials from {}", _wifi_config_path);

    FILE* f = fopen(_wifi_config_path.c_str(), "rb");
    if (!f) {
        mclog::tagWarn(_tag, "no wifi_config.json yet, trying env vars");
        _maybe_seed_from_env();
        if (hasCredentials()) {
            save();
        }
        return hasCredentials();
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string content;
    content.resize(static_cast<size_t>(sz));
    fread(content.data(), 1, static_cast<size_t>(sz), f);
    fclose(f);

    JsonDocument doc;
    if (deserializeJson(doc, content) != DeserializationError::Ok) {
        mclog::tagError(_tag, "parse wifi_config.json failed");
        return false;
    }

    _ssid = doc["ssid"].as<std::string>();
    _password = doc["password"].as<std::string>();
    mclog::tagInfo(_tag, "loaded ssid: {}", _ssid.empty() ? "<empty>" : _ssid);
    return hasCredentials();
}

bool WifiManagerStd::save()
{
    JsonDocument doc;
    doc["ssid"] = _ssid;
    doc["password"] = _password;

    std::string out;
    if (serializeJson(doc, out) == 0) {
        mclog::tagError(_tag, "serialize wifi config failed");
        return false;
    }

    FILE* f = fopen(_wifi_config_path.c_str(), "wb");
    if (!f) {
        mclog::tagError(_tag, "open {} for write failed", _wifi_config_path);
        return false;
    }
    fputs(out.c_str(), f);
    fclose(f);
    mclog::tagInfo(_tag, "saved wifi config to {}", _wifi_config_path);
    return true;
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

void WifiManagerStd::_maybe_seed_from_env()
{
    const char* env_ssid = std::getenv("PHOEBE_WIFI_SSID");
    const char* env_pw = std::getenv("PHOEBE_WIFI_PASSWORD");
    if (env_ssid && env_ssid[0] != '\0') {
        _ssid = env_ssid;
        _password = env_pw ? env_pw : "";
        mclog::tagInfo(_tag, "seeded credentials from env (ssid={})", _ssid);
    }
}

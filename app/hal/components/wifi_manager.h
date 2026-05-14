/**
 * @file wifi_manager.h
 * @brief WiFi credential store and (simulated) connect/disconnect API.
 *
 * On real hardware (ESP32) implementations persist to NVS and drive the
 * actual radio. On the desktop simulator the impl persists to a JSON file
 * and `connect()` is a no-op stub that just reports "connected" if
 * credentials are present.
 */
#pragma once
#include <string>

namespace hal_components {

class WifiManagerBase {
public:
    virtual ~WifiManagerBase() = default;

    // Load persisted credentials, if any. Returns true on success.
    virtual bool load()
    {
        return false;
    }

    // Persist current credentials. Returns true on success.
    virtual bool save()
    {
        return false;
    }

    // Replace the stored credentials and persist them.
    virtual void setCredentials(const std::string& ssid, const std::string& password)
    {
        _ssid = ssid;
        _password = password;
    }

    const std::string& getSsid() const
    {
        return _ssid;
    }

    const std::string& getPassword() const
    {
        return _password;
    }

    bool hasCredentials() const
    {
        return !_ssid.empty();
    }

    // Begin a connection attempt with the stored credentials. Returns true
    // once the link is up (or, on the desktop sim, immediately when
    // credentials are present).
    virtual bool connect()
    {
        return hasCredentials();
    }

    virtual void disconnect() {}

    virtual bool isConnected() const
    {
        return false;
    }

    virtual void logState() const {}

protected:
    std::string _ssid;
    std::string _password;
};

} // namespace hal_components

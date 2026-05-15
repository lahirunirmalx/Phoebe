/**
 * @file wifi_manager_esp32.cpp
 * @brief Arduino-WiFi-backed wifi manager. NVS for credentials.
 *        Matches the WiFi usage style in M5Cardputer-UserDemo so the same
 *        Arduino HTTPClient stack works on top.
 */
#include "wifi_manager_esp32.h"
#include <WiFi.h>
#include <mooncake_log.h>
#include <nvs.h>
#include <time.h>

static const char* _tag = "wifi";
static const char* NVS_NS = "wifi";
static const char* NVS_KEY_SSID = "ssid";
static const char* NVS_KEY_PW = "password";

volatile bool WifiManagerEsp32::_s_connected = false;

bool WifiManagerEsp32::load()
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        mclog::tagWarn(_tag, "no nvs entries yet");
        return false;
    }

    size_t sz;
    char buf[64];

    sz = sizeof(buf);
    if (nvs_get_str(h, NVS_KEY_SSID, buf, &sz) == ESP_OK) _ssid = buf;
    sz = sizeof(buf);
    if (nvs_get_str(h, NVS_KEY_PW, buf, &sz) == ESP_OK) _password = buf;
    nvs_close(h);

    mclog::tagInfo(_tag, "loaded ssid: {}", _ssid.empty() ? "<empty>" : _ssid);
    return hasCredentials();
}

bool WifiManagerEsp32::save()
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        mclog::tagError(_tag, "nvs_open write failed");
        return false;
    }
    nvs_set_str(h, NVS_KEY_SSID, _ssid.c_str());
    nvs_set_str(h, NVS_KEY_PW, _password.c_str());
    nvs_commit(h);
    nvs_close(h);
    mclog::tagInfo(_tag, "saved to nvs");
    return true;
}

void WifiManagerEsp32::setCredentials(const std::string& ssid, const std::string& password)
{
    WifiManagerBase::setCredentials(ssid, password);
    save();
    _s_connected = false;
}

void WifiManagerEsp32::_ensure_init()
{
    if (_wifi_initialised) return;
    WiFi.mode(WIFI_STA);
    _wifi_initialised = true;
}

bool WifiManagerEsp32::connect()
{
    if (!hasCredentials()) {
        mclog::tagWarn(_tag, "no credentials, cannot connect");
        return false;
    }
    _ensure_init();

    mclog::tagInfo(_tag, "wifi begin, ssid={}", _ssid);
    WiFi.begin(_ssid.c_str(), _password.c_str());

    // ESP32 has no RTC -- sync system time from NTP as soon as we're online.
    // configTime() is the Arduino-ESP32 thin wrapper around esp_sntp. It
    // returns immediately; the actual sync happens once DHCP completes.
    // Offsets are 0/0 (UTC); set them or call setenv("TZ", ...) elsewhere
    // if you want local time on the clock face.
    mclog::tagInfo(_tag, "sntp begin (pool.ntp.org)");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    return true;
}

void WifiManagerEsp32::disconnect()
{
    if (!_wifi_initialised) return;
    WiFi.disconnect(true);
    _s_connected = false;
}

bool WifiManagerEsp32::isConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

void WifiManagerEsp32::logState() const
{
    mclog::tagInfo(_tag, "ssid: {}  has_password: {}  connected: {}",
                   _ssid.empty() ? "<unset>" : _ssid, !_password.empty(),
                   WiFi.status() == WL_CONNECTED);
}

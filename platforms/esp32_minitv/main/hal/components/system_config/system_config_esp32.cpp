/**
 * @file system_config_esp32.cpp
 * @brief See system_config_esp32.h.
 */
#include "system_config_esp32.h"
#include <nvs.h>
#include <mooncake_log.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace {
const char* TAG = "syscfg";

// Read an NVS string key into `out`; leaves out unchanged on miss.
void nvs_get_string(nvs_handle_t h, const char* key, std::string& out)
{
    size_t sz = 0;
    if (nvs_get_str(h, key, nullptr, &sz) != ESP_OK || sz == 0) return;
    std::string buf(sz, '\0');
    if (nvs_get_str(h, key, buf.data(), &sz) == ESP_OK) {
        if (!buf.empty() && buf.back() == '\0') buf.pop_back();
        out = buf;
    }
}

bool nvs_get_bool(nvs_handle_t h, const char* key, bool fallback)
{
    uint8_t v = 0;
    if (nvs_get_u8(h, key, &v) == ESP_OK) return v != 0;
    return fallback;
}
} // namespace

bool SystemConfigEsp32::loadConfig()
{
    nvs_handle_t h;

    if (nvs_open("wifi", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_string(h, "ssid", _config.wifiSsid);
        nvs_get_string(h, "password", _config.wifiPassword);
        nvs_close(h);
    }
    if (nvs_open("claude", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_string(h, "base", _config.claudeBase);
        nvs_get_string(h, "bearer", _config.claudeBearer);
        nvs_close(h);
    }
    if (nvs_open("phoebe", NVS_READONLY, &h) == ESP_OK) {
        _config.mute = nvs_get_bool(h, "mute", _config.mute);
        _config.hapticFeedback = nvs_get_bool(h, "haptic", _config.hapticFeedback);
        nvs_get_string(h, "watchFace", _config.watchFace);
        nvs_get_string(h, "widgetA", _config.widgetA);
        nvs_get_string(h, "widgetB", _config.widgetB);
        nvs_get_string(h, "city", _config.weatherCity);
        nvs_get_string(h, "ics", _config.icsUrl);
        nvs_get_string(h, "urls", _config.uptimeUrls);
        int32_t tz = _config.tzOffsetMin;
        if (nvs_get_i32(h, "tz", &tz) == ESP_OK) _config.tzOffsetMin = tz;
        nvs_close(h);
    }

    if (_config.watchFace.empty()) _config.watchFace = "vfd"; // sensible default
    applyConfig(); // set TZ now (re-applied after wifi/SNTP in HAL init)
    logConfig();
    return true;
}

bool SystemConfigEsp32::saveConfig()
{
    nvs_handle_t h;
    bool ok = true;

    if (nvs_open("wifi", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "ssid", _config.wifiSsid.c_str());
        nvs_set_str(h, "password", _config.wifiPassword.c_str());
        nvs_commit(h);
        nvs_close(h);
    } else ok = false;

    if (nvs_open("claude", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "base", _config.claudeBase.c_str());
        nvs_set_str(h, "bearer", _config.claudeBearer.c_str());
        nvs_commit(h);
        nvs_close(h);
    } else ok = false;

    if (nvs_open("phoebe", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "mute", _config.mute ? 1 : 0);
        nvs_set_u8(h, "haptic", _config.hapticFeedback ? 1 : 0);
        nvs_set_str(h, "watchFace", _config.watchFace.c_str());
        nvs_set_str(h, "widgetA", _config.widgetA.c_str());
        nvs_set_str(h, "widgetB", _config.widgetB.c_str());
        nvs_set_str(h, "city", _config.weatherCity.c_str());
        nvs_set_str(h, "ics", _config.icsUrl.c_str());
        nvs_set_str(h, "urls", _config.uptimeUrls.c_str());
        nvs_set_i32(h, "tz", _config.tzOffsetMin);
        nvs_commit(h);
        nvs_close(h);
    } else ok = false;

    mclog::tagInfo(TAG, "saveConfig {}", ok ? "ok" : "FAILED");
    return ok;
}

bool SystemConfigEsp32::applyConfig()
{
    // Build a fixed-offset POSIX TZ string. POSIX offsets are inverted (a
    // positive value is WEST of UTC), so an east (+) offset gets a '-' sign.
    const int off = _config.tzOffsetMin;
    const int ah = (off < 0 ? -off : off) / 60;
    const int am = (off < 0 ? -off : off) % 60;
    char tz[32];
    std::snprintf(tz, sizeof(tz), "GMT%c%d:%02d", (off >= 0 ? '-' : '+'), ah, am);
    setenv("TZ", tz, 1);
    tzset();
    mclog::tagInfo("syscfg", "timezone offset={} min -> TZ={}", off, tz);
    return true;
}

void SystemConfigEsp32::logConfig()
{
    mclog::tagInfo(TAG, "watchFace={} widgetA={} widgetB={} mute={} haptic={} ssid='{}' claudeBase='{}' hasBearer={}",
                   _config.watchFace, _config.widgetA, _config.widgetB, _config.mute,
                   _config.hapticFeedback, _config.wifiSsid,
                   _config.claudeBase.empty() ? "<unset>" : _config.claudeBase,
                   !_config.claudeBearer.empty());
}

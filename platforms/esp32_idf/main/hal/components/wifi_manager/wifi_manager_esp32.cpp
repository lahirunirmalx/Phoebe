/**
 * @file wifi_manager_esp32.cpp
 */
#include "wifi_manager_esp32.h"
#include <cstring>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <mooncake_log.h>
#include <nvs.h>
#include <nvs_flash.h>

static const char* _tag = "wifi";
static const char* NVS_NS = "wifi";
static const char* NVS_KEY_SSID = "ssid";
static const char* NVS_KEY_PW = "password";

volatile bool WifiManagerEsp32::_s_connected = false;

static void wifi_event_handler(void* /*arg*/, esp_event_base_t event_base, int32_t event_id, void* /*data*/)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        WifiManagerEsp32::_s_connected = false;
        mclog::tagWarn(_tag, "disconnected, retrying");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        WifiManagerEsp32::_s_connected = true;
        mclog::tagInfo(_tag, "got ip");
    }
}

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

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr,
                                        nullptr);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr,
                                        nullptr);
    esp_wifi_set_mode(WIFI_MODE_STA);

    _wifi_initialised = true;
}

bool WifiManagerEsp32::connect()
{
    if (!hasCredentials()) {
        mclog::tagWarn(_tag, "no credentials, cannot connect");
        return false;
    }
    _ensure_init();

    wifi_config_t wifi_cfg = {};
    std::strncpy(reinterpret_cast<char*>(wifi_cfg.sta.ssid), _ssid.c_str(), sizeof(wifi_cfg.sta.ssid));
    std::strncpy(reinterpret_cast<char*>(wifi_cfg.sta.password), _password.c_str(),
                 sizeof(wifi_cfg.sta.password));
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();

    mclog::tagInfo(_tag, "wifi started, ssid={}", _ssid);
    return true;
}

void WifiManagerEsp32::disconnect()
{
    if (!_wifi_initialised) return;
    esp_wifi_disconnect();
    _s_connected = false;
}

bool WifiManagerEsp32::isConnected() const
{
    return _s_connected;
}

void WifiManagerEsp32::logState() const
{
    mclog::tagInfo(_tag, "ssid: {}  has_password: {}  connected: {}",
                   _ssid.empty() ? "<unset>" : _ssid, !_password.empty(), (bool)_s_connected);
}

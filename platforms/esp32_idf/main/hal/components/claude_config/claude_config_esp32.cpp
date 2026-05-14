/**
 * @file claude_config_esp32.cpp
 */
#include "claude_config_esp32.h"
#include <mooncake_log.h>
#include <nvs.h>
#include <nvs_flash.h>

static const char* _tag = "claudecfg";
static const char* NVS_NS = "claude";
static const char* NVS_KEY_BASE = "base";
static const char* NVS_KEY_BEARER = "bearer";

bool ClaudeConfigEsp32::load()
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        mclog::tagWarn(_tag, "no nvs entries yet");
        return false;
    }

    char buf[160];
    size_t sz;

    sz = sizeof(buf);
    if (nvs_get_str(h, NVS_KEY_BASE, buf, &sz) == ESP_OK) _base_url = buf;
    sz = sizeof(buf);
    if (nvs_get_str(h, NVS_KEY_BEARER, buf, &sz) == ESP_OK) _bearer = buf;
    nvs_close(h);

    mclog::tagInfo(_tag, "loaded base: {}  has_bearer: {}",
                   _base_url.empty() ? "<unset>" : _base_url, !_bearer.empty());
    return isReady();
}

bool ClaudeConfigEsp32::save()
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        mclog::tagError(_tag, "nvs_open write failed");
        return false;
    }
    nvs_set_str(h, NVS_KEY_BASE, _base_url.c_str());
    nvs_set_str(h, NVS_KEY_BEARER, _bearer.c_str());
    nvs_commit(h);
    nvs_close(h);
    mclog::tagInfo(_tag, "saved to nvs");
    return true;
}

void ClaudeConfigEsp32::logState() const
{
    mclog::tagInfo(_tag, "base: {}  has_bearer: {}",
                   _base_url.empty() ? "<unset>" : _base_url, !_bearer.empty());
}

/**
 * @file claude_config_std.cpp
 */
#include "claude_config_std.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <cstdlib>
#include <mooncake_log.h>

static const char* _tag = "claudecfg";

ClaudeConfigStd::ClaudeConfigStd(const std::string& rootPath)
{
    _path = rootPath + "claude_config.json";
}

bool ClaudeConfigStd::load()
{
    FILE* f = fopen(_path.c_str(), "rb");
    if (!f) {
        mclog::tagWarn(_tag, "no claude_config.json yet, trying env vars");
        _maybe_seed_from_env();
        if (isReady()) save();
        return isReady();
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
        mclog::tagError(_tag, "parse failed for {}", _path);
        return false;
    }

    _base_url = doc["base"].as<std::string>();
    _bearer = doc["bearer"].as<std::string>();

    // Allow env vars to override on every run if explicitly set.
    _maybe_seed_from_env();

    mclog::tagInfo(_tag, "loaded base: {}  bearer: {}",
                   _base_url.empty() ? "<unset>" : _base_url, _bearer.empty() ? "<unset>" : "***");
    return isReady();
}

bool ClaudeConfigStd::save()
{
    JsonDocument doc;
    doc["base"] = _base_url;
    doc["bearer"] = _bearer;

    std::string out;
    if (serializeJson(doc, out) == 0) {
        mclog::tagError(_tag, "serialize failed");
        return false;
    }

    FILE* f = fopen(_path.c_str(), "wb");
    if (!f) {
        mclog::tagError(_tag, "open {} for write failed", _path);
        return false;
    }
    fputs(out.c_str(), f);
    fclose(f);
    mclog::tagInfo(_tag, "saved to {}", _path);
    return true;
}

void ClaudeConfigStd::logState() const
{
    mclog::tagInfo(_tag, "base: {}  has_bearer: {}",
                   _base_url.empty() ? "<unset>" : _base_url, !_bearer.empty());
}

void ClaudeConfigStd::_maybe_seed_from_env()
{
    const char* env_base = std::getenv("PHOEBE_CLAUDE_BASE");
    const char* env_bearer = std::getenv("PHOEBE_CLAUDE_BEARER");
    if (env_base && env_base[0] != '\0') _base_url = env_base;
    if (env_bearer && env_bearer[0] != '\0') _bearer = env_bearer;
}

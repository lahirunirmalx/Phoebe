/**
 * @file claude_config.h
 * @brief Storage for the Claude usage API base URL and bearer token.
 *
 * Mirrors the M5Cardputer-UserDemo claudemeter's NVS layout (namespace
 * "claude", keys "base" and "bearer"). Implementations persist somewhere
 * platform-appropriate -- a JSON file on desktop, NVS on ESP32 -- but the
 * shape of the data is the same.
 */
#pragma once
#include <string>

namespace hal_components {

class ClaudeConfigBase {
public:
    virtual ~ClaudeConfigBase() = default;

    virtual bool load()
    {
        return false;
    }
    virtual bool save()
    {
        return false;
    }

    virtual void setBaseUrl(const std::string& url)
    {
        _base_url = url;
    }
    virtual void setBearer(const std::string& bearer)
    {
        _bearer = bearer;
    }

    const std::string& getBaseUrl() const
    {
        return _base_url;
    }
    const std::string& getBearer() const
    {
        return _bearer;
    }

    bool isReady() const
    {
        return !_base_url.empty() && !_bearer.empty();
    }

    virtual void logState() const {}

protected:
    std::string _base_url;
    std::string _bearer;
};

} // namespace hal_components

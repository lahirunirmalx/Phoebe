/**
 * @file http_client_arduino.h
 * @brief Arduino HTTPClient-backed GET for ESP32. Mirrors what
 *        M5Cardputer-UserDemo/main/apps/app_claudemeter uses inline.
 */
#pragma once
#include "hal/components/http_client.h"

class HttpClientArduino : public hal_components::HttpClientBase {
public:
    Response get(const std::string& url, const std::string& bearerToken, int timeoutSec = 8) override;
    int getLines(const std::string& url, const std::string& bearerToken, int timeoutSec,
                 const std::function<void(const char*)>& on_line) override;
    int status(const std::string& url, int timeoutSec = 8) override;
    int measureDownload(const std::string& url, int timeoutSec, int maxBytes) override;
};

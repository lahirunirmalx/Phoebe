/**
 * @file http_client_curl.h
 * @brief libcurl-backed HTTP GET for the desktop sim.
 */
#pragma once
#include "hal/components/http_client.h"

class HttpClientCurl : public hal_components::HttpClientBase {
public:
    Response get(const std::string& url, const std::string& bearerToken, int timeoutSec = 8) override;
};

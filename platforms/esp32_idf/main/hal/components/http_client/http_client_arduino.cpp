/**
 * @file http_client_arduino.cpp
 */
#include "http_client_arduino.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <cstdio>

hal_components::HttpClientBase::Response HttpClientArduino::get(const std::string& url,
                                                                const std::string& bearerToken,
                                                                int timeoutSec)
{
    Response out;

    if (WiFi.status() != WL_CONNECTED) {
        out.error = "no wifi";
        return out;
    }

    HTTPClient http;
    http.setTimeout(timeoutSec * 1000);
    if (!http.begin(url.c_str())) {
        out.error = "begin failed";
        return out;
    }

    if (!bearerToken.empty()) {
        std::string auth = "Bearer " + bearerToken;
        http.addHeader("Authorization", auth.c_str());
    }

    int code = http.GET();
    out.http_code = code;
    if (code >= 0) {
        out.body = std::string(http.getString().c_str());
    } else {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "transport %d", code);
        out.error = buf;
    }
    http.end();
    return out;
}

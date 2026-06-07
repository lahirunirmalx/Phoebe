/**
 * @file http_client_arduino.cpp
 */
#include "http_client_arduino.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cstdio>
#include <string>

hal_components::HttpClientBase::Response HttpClientArduino::get(const std::string& url_in,
                                                                const std::string& bearerToken,
                                                                int timeoutSec)
{
    Response out;

    if (WiFi.status() != WL_CONNECTED) {
        out.error = "no wifi";
        return out;
    }

    // Tolerate a base URL entered without a scheme (e.g. "host:7878") -- default
    // to http:// so http.begin() doesn't fail on a missing scheme.
    std::string url = url_in;
    const bool is_https = url.rfind("https://", 0) == 0;
    if (!is_https && url.rfind("http://", 0) != 0) {
        url = "http://" + url;
    }

    HTTPClient http;
    http.setTimeout(timeoutSec * 1000);

    // Use the modern begin(client, url) form. A local exporter rarely has a CA
    // the ESP trusts, so accept any cert on https.
    bool begun;
    if (is_https) {
        static WiFiClientSecure secure;
        secure.setInsecure();
        secure.setHandshakeTimeout(timeoutSec); // bound the TLS handshake (else it can hang)
        begun = http.begin(secure, url.c_str());
    } else {
        static WiFiClient client;
        begun = http.begin(client, url.c_str());
    }
    if (!begun) {
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

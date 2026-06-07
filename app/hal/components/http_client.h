/**
 * @file http_client.h
 * @brief Tiny HTTP GET abstraction shared across platforms.
 *        Desktop impl uses libcurl. ESP32 impl uses Arduino HTTPClient,
 *        matching the M5Cardputer-UserDemo approach.
 */
#pragma once
#include <functional>
#include <string>

namespace hal_components {

class HttpClientBase {
public:
    struct Response {
        int http_code = 0; // 0 means transport-level failure
        std::string body;
        std::string error; // short human-readable transport error
        bool ok() const
        {
            return http_code == 200;
        }
    };

    virtual ~HttpClientBase() = default;

    // GET with optional "Bearer ..." auth header (without the "Authorization: " prefix).
    // timeoutSec is the whole-request timeout.
    virtual Response get(const std::string& /*url*/, const std::string& /*bearerToken*/,
                         int /*timeoutSec*/ = 8)
    {
        return {0, "", "not implemented"};
    }

    // Streaming GET: invokes on_line for each line of the (de-chunked) body
    // without holding the whole response in RAM -- for large feeds like iCal.
    // Returns the HTTP status code (0 = transport failure).
    virtual int getLines(const std::string& /*url*/, const std::string& /*bearerToken*/,
                         int /*timeoutSec*/, const std::function<void(const char*)>& /*on_line*/)
    {
        return 0;
    }

    // Status-only request: returns the HTTP status code WITHOUT downloading the
    // body (for uptime/ping checks against arbitrary sites with large pages).
    // 0 = transport failure.
    virtual int status(const std::string& /*url*/, int /*timeoutSec*/ = 8)
    {
        return 0;
    }
};

} // namespace hal_components

/**
 * @file http_client_arduino.cpp
 */
#include "http_client_arduino.h"
#include <HTTPClient.h>
#include <Stream.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cstdio>
#include <functional>
#include <string>

// A write-only Stream that buffers bytes into lines and invokes a callback per
// line. Used with HTTPClient::writeToStream() so a large (chunked) body is
// de-chunked and parsed line-by-line without ever holding the whole thing.
class LineSink : public Stream {
public:
    explicit LineSink(const std::function<void(const char*)>& cb) : _cb(cb) { _line.reserve(160); }
    size_t write(uint8_t b) override
    {
        if (b == '\n') {
            if (!_line.empty() && _line.back() == '\r') _line.pop_back();
            _cb(_line.c_str());
            _line.clear();
        } else if (_line.size() < 1024) { // guard against a pathological long line
            _line.push_back((char)b);
        }
        return 1;
    }
    size_t write(const uint8_t* buf, size_t n) override
    {
        for (size_t i = 0; i < n; ++i) write(buf[i]);
        return n;
    }
    void flush() override { if (!_line.empty()) { _cb(_line.c_str()); _line.clear(); } }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }

private:
    const std::function<void(const char*)>& _cb;
    std::string _line;
};

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

    // A TLS handshake needs a large contiguous block; if heap is low, fail
    // gracefully rather than letting operator new throw -> abort -> reboot.
    if (is_https && ESP.getFreeHeap() < 50000) { out.error = "low mem"; return out; }

    HTTPClient http;
    http.setTimeout(timeoutSec * 1000);

    // SECURITY: setInsecure() (below) skips TLS certificate validation for ALL
    // https requests -- the Claude exporter and the public weather/AQI/currency
    // APIs alike. A local exporter rarely has a CA the ESP trusts and this toy
    // has no managed cert store, so we accept any cert. Tradeoff: those feeds are
    // MITM-able; the bearer token still gates the Claude endpoint. To harden,
    // pin per-host CA certs via secure.setCACert(...) selected on the URL host.
    // Local (not static) clients so concurrent fetches from different tasks
    // don't share one connection object. Only one TLS context is alive per call.
    bool begun;
    WiFiClientSecure secure;
    WiFiClient client;
    if (is_https) {
        secure.setInsecure();
        secure.setHandshakeTimeout(timeoutSec); // bound the TLS handshake (else it can hang)
        begun = http.begin(secure, url.c_str());
    } else {
        begun = http.begin(client, url.c_str());
    }
    if (!begun) {
        out.error = "begin failed";
        return out;
    }

    // Some servers reject requests without a User-Agent (403/406) -- present a
    // browser-like one.
    http.setUserAgent("Mozilla/5.0 (compatible; phoebe-minitv/1.0)");

    if (!bearerToken.empty()) {
        std::string auth = "Bearer " + bearerToken;
        http.addHeader("Authorization", auth.c_str());
    }

    int code = http.GET();
    out.http_code = code;
    if (code >= 0) {
        // getString() handles chunked-transfer decoding. Use this only for the
        // small JSON/204 endpoints; large feeds (iCal) go through getLines().
        out.body = std::string(http.getString().c_str());
    } else {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "transport %d", code);
        out.error = buf;
    }
    http.end();
    return out;
}

int HttpClientArduino::status(const std::string& url_in, int timeoutSec)
{
    if (WiFi.status() != WL_CONNECTED) return 0;

    std::string url = url_in;
    const bool is_https = url.rfind("https://", 0) == 0;
    if (!is_https && url.rfind("http://", 0) != 0) url = "http://" + url;

    if (is_https && ESP.getFreeHeap() < 50000) return 0;

    HTTPClient http;
    http.setTimeout(timeoutSec * 1000);
    http.setConnectTimeout(timeoutSec * 1000);
    bool begun;
    WiFiClientSecure secure;
    WiFiClient client;
    if (is_https) {
        secure.setInsecure();
        secure.setHandshakeTimeout(timeoutSec);
        begun = http.begin(secure, url.c_str());
    } else {
        begun = http.begin(client, url.c_str());
    }
    if (!begun) return 0;
    http.setUserAgent("Mozilla/5.0 (compatible; phoebe-minitv/1.0)");

    // GET returns the status once headers arrive; we deliberately never read the
    // body (a real web page can be hundreds of KB -> OOM). end() drops it.
    int code = http.GET();
    http.end();
    return code;
}

int HttpClientArduino::getLines(const std::string& url_in, const std::string& bearerToken,
                                int timeoutSec, const std::function<void(const char*)>& on_line)
{
    if (WiFi.status() != WL_CONNECTED) return 0;

    std::string url = url_in;
    const bool is_https = url.rfind("https://", 0) == 0;
    if (!is_https && url.rfind("http://", 0) != 0) url = "http://" + url;

    if (is_https && ESP.getFreeHeap() < 50000) return 0; // low mem -> skip

    HTTPClient http;
    http.setTimeout(timeoutSec * 1000);
    bool begun;
    WiFiClientSecure secure;
    WiFiClient client;
    if (is_https) {
        secure.setInsecure();
        secure.setHandshakeTimeout(timeoutSec);
        begun = http.begin(secure, url.c_str());
    } else {
        begun = http.begin(client, url.c_str());
    }
    if (!begun) return 0;
    http.setUserAgent("Mozilla/5.0 (compatible; phoebe-minitv/1.0)");
    if (!bearerToken.empty()) http.addHeader("Authorization", ("Bearer " + bearerToken).c_str());

    int code = http.GET();
    if (code >= 0) {
        // De-chunk via writeToStream() into a sink that emits whole lines, so
        // the full body is never held in RAM -- only one line at a time.
        LineSink sink(on_line);
        http.writeToStream(&sink);
        sink.flush();
    }
    http.end();
    return code;
}

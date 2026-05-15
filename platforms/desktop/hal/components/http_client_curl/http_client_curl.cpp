/**
 * @file http_client_curl.cpp
 */
#include "http_client_curl.h"
#include <curl/curl.h>
#include <cstdio>

namespace {
size_t collect(void* contents, size_t size, size_t nmemb, void* userp)
{
    auto* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}
} // namespace

hal_components::HttpClientBase::Response HttpClientCurl::get(const std::string& url,
                                                             const std::string& bearerToken, int timeoutSec)
{
    Response out;

    CURL* curl = curl_easy_init();
    if (!curl) {
        out.error = "curl init";
        return out;
    }

    struct curl_slist* headers = nullptr;
    std::string auth;
    if (!bearerToken.empty()) {
        auth = "Authorization: Bearer " + bearerToken;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeoutSec);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode rc = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    out.http_code = static_cast<int>(http_code);

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        out.error = std::string("net: ") + curl_easy_strerror(rc);
    }
    return out;
}

#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <curl/curl.h>

struct HttpResponse {
    std::vector<char> body;
    std::string content_type;
    long status_code = 0;
    CURLcode curl_code = CURLE_OK;
};

using ProgressFn = std::function<void(double fraction)>;

class HttpClient {
public:
    HttpClient() : _curl(curl_easy_init()) {
        if (!_curl) {
            throw std::runtime_error("failed to init cURL");
        }
    }

    ~HttpClient() {
        if (this->_curl) {
            curl_easy_cleanup(this->_curl);
        }
    }

    HttpClient(const HttpClient &) = delete;
    HttpClient &operator=(const HttpClient &) = delete;

    HttpResponse get(const std::string &url, const ProgressFn &on_progress = {}) const {
        HttpResponse response;

        curl_easy_setopt(this->_curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(this->_curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(this->_curl, CURLOPT_WRITEDATA, &response.body);
        curl_easy_setopt(this->_curl, CURLOPT_TIMEOUT, 5L);
        curl_easy_setopt(this->_curl, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(this->_curl, CURLOPT_FAILONERROR, 1L);

        if (on_progress) {
            curl_easy_setopt(this->_curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(this->_curl, CURLOPT_XFERINFOFUNCTION, progress_cb);
            curl_easy_setopt(this->_curl, CURLOPT_XFERINFODATA, &on_progress);
        } else {
            curl_easy_setopt(this->_curl, CURLOPT_NOPROGRESS, 1L);
        }

        response.curl_code = curl_easy_perform(this->_curl);

        char *ct = nullptr;
        if (curl_easy_getinfo(this->_curl, CURLINFO_CONTENT_TYPE, &ct) == CURLE_OK && ct) {
            response.content_type = ct;
        }

        curl_easy_getinfo(this->_curl, CURLINFO_RESPONSE_CODE, &response.status_code);

        return response;
    }

    bool is_image(const HttpResponse &response) const {
        return response.content_type.starts_with("image");
    }

private:
    CURL *_curl;

    static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
        auto &buf = *static_cast<std::vector<char> *>(userdata);
        size_t total = size * nmemb;
        buf.insert(buf.end(), static_cast<char *>(ptr), static_cast<char *>(ptr) + total);
        return total;
    }

    static int progress_cb(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                            curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
        const auto &fn = *static_cast<const ProgressFn *>(clientp);
        if (dltotal > 0) {
            fn(double(dlnow) / double(dltotal));
        } else {
            fn(-1.0);
        }
        return 0;
    }
};

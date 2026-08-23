#pragma once

#include <exception>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
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
    using WriteFn = std::function<void(std::vector<char> &, const char *, size_t)>;

    explicit HttpClient(WriteFn write = {})
        : _curl(curl_easy_init()), _write(std::move(write)) {
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
        WriteContext write_context = {&response.body, &this->_write, {}};
        ProgressContext progress_context = {&on_progress, {}};

        curl_easy_setopt(this->_curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(this->_curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(this->_curl, CURLOPT_WRITEDATA, &write_context);
        curl_easy_setopt(this->_curl, CURLOPT_TIMEOUT, 5L);
        curl_easy_setopt(this->_curl, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(this->_curl, CURLOPT_FAILONERROR, 1L);

        if (on_progress) {
            curl_easy_setopt(this->_curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(this->_curl, CURLOPT_XFERINFOFUNCTION, progress_cb);
            curl_easy_setopt(this->_curl, CURLOPT_XFERINFODATA, &progress_context);
        } else {
            curl_easy_setopt(this->_curl, CURLOPT_NOPROGRESS, 1L);
        }

        response.curl_code = curl_easy_perform(this->_curl);
        if (write_context.exception) {
            std::rethrow_exception(write_context.exception);
        }
        if (progress_context.exception) {
            std::rethrow_exception(progress_context.exception);
        }

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
    struct WriteContext {
        std::vector<char> *buffer;
        const WriteFn *write;
        std::exception_ptr exception;
    };

    struct ProgressContext {
        const ProgressFn *progress;
        std::exception_ptr exception;
    };

    CURL *_curl;
    WriteFn _write;

    static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) noexcept {
        auto &context = *static_cast<WriteContext *>(userdata);
        const size_t total = size * nmemb;
        try {
            const auto *data = static_cast<const char *>(ptr);
            if (*context.write) {
                (*context.write)(*context.buffer, data, total);
            } else {
                context.buffer->insert(context.buffer->end(), data, data + total);
            }
            return total;
        } catch (...) {
            context.exception = std::current_exception();
            return 0;
        }
    }

    static int progress_cb(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                            curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) noexcept {
        auto &context = *static_cast<ProgressContext *>(clientp);
        try {
            if (dltotal > 0) {
                (*context.progress)(double(dlnow) / double(dltotal));
            } else {
                (*context.progress)(-1.0);
            }
            return 0;
        } catch (...) {
            context.exception = std::current_exception();
            return 1;
        }
    }
};

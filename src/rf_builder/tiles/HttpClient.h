#pragma once
#include "Error.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <curl/curl.h>
#include <optional>
#include <string>
#include <vector>

namespace rf_builder::tiles {
struct NetworkCounters {
    std::atomic<std::uint64_t> requests = 0;
    std::atomic<std::uint64_t> bytes = 0;
};
struct RetryPolicy {
    std::chrono::milliseconds initial_wait { 500 };
    std::chrono::milliseconds deadline = std::chrono::hours(1);
    std::chrono::milliseconds request_timeout = std::chrono::seconds(30);
    std::chrono::milliseconds connect_timeout = std::chrono::seconds(10);
};
class HttpClient {
public:
    HttpClient(NetworkCounters& counters, std::size_t response_limit, RetryPolicy policy = {});
    ~HttpClient();
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    Expected<std::optional<std::vector<std::uint8_t>>> get(const std::string& url);

private:
    CURL* m_curl;
    NetworkCounters& m_counters;
    std::size_t m_response_limit;
    RetryPolicy m_policy;
};
} // namespace rf_builder::tiles

/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#pragma once
#include "Error.h"
#include <atomic>
#include <chrono>
#include <cstddef>
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
    Expected<std::optional<std::vector<std::byte>>> get(const std::string& url);

private:
    CURL* m_curl;
    NetworkCounters& m_counters;
    std::size_t m_response_limit;
    RetryPolicy m_policy;
};
} // namespace rf_builder::tiles

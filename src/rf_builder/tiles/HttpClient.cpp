#include "HttpClient.h"
#include "log.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace rf_builder::tiles {
namespace {
    struct Response {
        std::vector<std::uint8_t> bytes;
        NetworkCounters* counters;
        std::size_t limit;
        bool too_large = false;
        std::exception_ptr exception;
        std::chrono::milliseconds retry_after { 0 };
    };
    std::size_t write(void* data, std::size_t size, std::size_t count, void* context) noexcept
    {
        auto& response = *static_cast<Response*>(context);
        if (size && count > (std::numeric_limits<std::size_t>::max)() / size) {
            response.too_large = true;
            return 0;
        }
        const auto bytes = size * count;
        response.counters->bytes.fetch_add(bytes, std::memory_order_relaxed);
        if (bytes > response.limit - response.bytes.size()) {
            response.too_large = true;
            return 0;
        }
        try {
            const auto* first = static_cast<std::uint8_t*>(data);
            response.bytes.insert(response.bytes.end(), first, first + bytes);
            return bytes;
        } catch (...) {
            response.exception = std::current_exception();
            return 0;
        }
    }
    std::size_t header(char* data, std::size_t size, std::size_t count, void* context) noexcept
    {
        auto& response = *static_cast<Response*>(context);
        const std::size_t bytes = size * count;
        try {
            std::string_view line(data, bytes);
            if (line.starts_with("HTTP/")) {
                response.retry_after = std::chrono::milliseconds(0);
                response.bytes.clear();
                return bytes;
            }
            const auto colon = line.find(':');
            if (colon == std::string_view::npos) {
                return bytes;
            }
            std::string name(line.substr(0, colon));
            std::ranges::transform(name, name.begin(), [](unsigned char value) { return char(std::tolower(value)); });
            if (name != "retry-after") {
                return bytes;
            }
            auto value = line.substr(colon + 1);
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
                value.remove_prefix(1);
            }
            while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ')) {
                value.remove_suffix(1);
            }
            std::uint64_t seconds = 0;
            auto parsed = std::from_chars(value.data(), value.data() + value.size(), seconds);
            if (parsed.ec != std::errc() || parsed.ptr != value.data() + value.size()) {
                const auto date = curl_getdate(std::string(value).c_str(), nullptr);
                const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                seconds = date > now ? std::uint64_t(date - now) : 0;
            }
            response.retry_after = std::chrono::milliseconds((std::min)(seconds, std::uint64_t(3600)) * 1000);
            return bytes;
        } catch (...) {
            response.exception = std::current_exception();
            return 0;
        }
    }
    bool retryable(CURLcode code, long status)
    {
        if (code == CURLE_OK) {
            return status == 408 || status == 429 || status == 500 || status == 502 || status == 503 || status == 504;
        }
        constexpr std::array transient { CURLE_COULDNT_RESOLVE_PROXY,
            CURLE_COULDNT_RESOLVE_HOST,
            CURLE_COULDNT_CONNECT,
            CURLE_OPERATION_TIMEDOUT,
            CURLE_RECV_ERROR,
            CURLE_SEND_ERROR,
            CURLE_GOT_NOTHING,
            CURLE_PARTIAL_FILE,
            CURLE_HTTP2,
            CURLE_HTTP2_STREAM };
        return std::ranges::find(transient, code) != transient.end();
    }
} // namespace
HttpClient::HttpClient(NetworkCounters& counters, std::size_t response_limit, RetryPolicy policy)
    : m_curl(nullptr)
    , m_counters(counters)
    , m_response_limit(response_limit)
    , m_policy(policy)
{
    static const auto initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (initialized != CURLE_OK || !(m_curl = curl_easy_init())) {
        throw std::runtime_error("initialize RF curl client");
    }
}
HttpClient::~HttpClient() { curl_easy_cleanup(m_curl); }
Expected<std::optional<std::vector<std::uint8_t>>> HttpClient::get(const std::string& url)
{
    if (m_policy.deadline.count() <= 0 || m_policy.initial_wait.count() <= 0 || m_policy.request_timeout.count() <= 0
        || m_policy.connect_timeout.count() <= 0) {
        return Error::fail(Error::Code::InvalidInput, "HTTP retry durations must be positive");
    }
    const auto deadline = std::chrono::steady_clock::now() + m_policy.deadline;
    auto delay = m_policy.initial_wait;
    std::uint64_t attempt = 0;
    for (;;) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) {
            LOG_ERROR("RF HTTP retry deadline exhausted after {} attempts: {}", attempt, url);
            return Error::fail(Error::Code::Io, "HTTP retry deadline exhausted for " + url);
        }
        Response response { .bytes = {}, .counters = &m_counters, .limit = m_response_limit, .exception = {} };
        curl_easy_reset(m_curl);
        const auto set = [&](CURLoption option, auto value) { return curl_easy_setopt(m_curl, option, value) == CURLE_OK; };
        if (!set(CURLOPT_URL, url.c_str()) || !set(CURLOPT_WRITEFUNCTION, write) || !set(CURLOPT_WRITEDATA, &response) || !set(CURLOPT_HEADERFUNCTION, header)
            || !set(CURLOPT_HEADERDATA, &response) || !set(CURLOPT_TIMEOUT_MS, long((std::min)(remaining, m_policy.request_timeout).count()))
            || !set(CURLOPT_CONNECTTIMEOUT_MS, long((std::min)(remaining, m_policy.connect_timeout).count())) || !set(CURLOPT_NOSIGNAL, 1L)
            || !set(CURLOPT_FOLLOWLOCATION, 1L) || !set(CURLOPT_MAXREDIRS, 5L) || !set(CURLOPT_PROTOCOLS_STR, "http,https")
            || !set(CURLOPT_REDIR_PROTOCOLS_STR, "http,https")) {
            return Error::fail(Error::Code::Internal, "configure RF HTTP request");
        }
        ++attempt;
        m_counters.requests.fetch_add(1, std::memory_order_relaxed);
        const auto code = curl_easy_perform(m_curl);
        if (response.exception) {
            return Error::fail(Error::Code::ResourceExhausted, "buffer HTTP response for " + url);
        }
        if (response.too_large) {
            return Error::fail(Error::Code::ResourceExhausted, "HTTP response exceeds bounded source buffer for " + url);
        }
        long status = 0;
        if (curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &status) != CURLE_OK) {
            return Error::fail(Error::Code::Internal, "read HTTP response status");
        }
        if (code == CURLE_OK && status == 404) {
            return std::nullopt;
        }
        if (code == CURLE_OK && status == 200) {
            return std::optional(std::move(response.bytes));
        }
        if (!retryable(code, status)) {
            return Error::fail(Error::Code::Io, fmt::format("HTTP request failed: status {}, transport {}; {}", status, curl_easy_strerror(code), url));
        }
        const auto budget = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        const auto wait = (std::max)(std::chrono::milliseconds(0), (std::min)(budget, (std::max)(delay, response.retry_after)));
        LOG_WARN("RF HTTP attempt {} failed: status {}, transport {}; retry wait {} ms, remaining budget {} ms; {}",
            attempt,
            status,
            curl_easy_strerror(code),
            wait.count(),
            (std::max)(std::int64_t(0), budget.count()),
            url);
        std::this_thread::sleep_for(wait);
        delay = delay >= m_policy.deadline / 2 ? m_policy.deadline : delay * 2;
    }
}
} // namespace rf_builder::tiles

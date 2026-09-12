#pragma once
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace rf_test {
struct Response {
    int status = 404;
    std::vector<std::uint8_t> body;
    std::string headers;
    std::chrono::milliseconds delay { 0 };
};
class HttpFixture {
public:
    using Handler = std::function<Response(const std::string&, unsigned)>;
    explicit HttpFixture(Handler handler)
        : m_handler(std::move(handler))
    {
        m_socket = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (m_socket < 0) {
            throw std::runtime_error("create test socket");
        }
        sockaddr_in address {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(m_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) || ::listen(m_socket, 32)) {
            ::close(m_socket);
            throw std::runtime_error("bind test socket");
        }
        socklen_t length = sizeof(address);
        if (::getsockname(m_socket, reinterpret_cast<sockaddr*>(&address), &length)) {
            ::close(m_socket);
            throw std::runtime_error("inspect test socket");
        }
        m_port = ntohs(address.sin_port);
        for (unsigned i = 0; i < 4; ++i) {
            m_threads.emplace_back([this] { serve(); });
        }
    }
    ~HttpFixture()
    {
        m_stopped = true;
        ::shutdown(m_socket, SHUT_RDWR);
        for (auto& thread : m_threads) {
            thread.join();
        }
        ::close(m_socket);
    }
    std::string base() const { return "http://127.0.0.1:" + std::to_string(m_port); }
    std::vector<std::string> requests() const
    {
        std::lock_guard lock(m_mutex);
        return m_requests;
    }
    void clear()
    {
        std::lock_guard lock(m_mutex);
        m_requests.clear();
        m_counts.clear();
    }
    void configure(const std::function<void()>& update)
    {
        std::lock_guard lock(m_mutex);
        update();
    }

private:
    void serve()
    {
        while (!m_stopped) {
            const int client = ::accept4(m_socket, nullptr, nullptr, SOCK_CLOEXEC);
            if (client < 0) {
                return;
            }
            timeval timeout { 2, 0 };
            ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            std::string request;
            char buffer[4096];
            while (request.find("\r\n\r\n") == std::string::npos && request.size() < 16384) {
                const auto count = ::recv(client, buffer, sizeof(buffer), 0);
                if (count <= 0) {
                    break;
                }
                request.append(buffer, std::size_t(count));
            }
            const auto first = request.find(' '), last = request.find(' ', first + 1);
            Response response;
            if (first != std::string::npos && last != std::string::npos) {
                const auto path = request.substr(first + 1, last - first - 1);
                std::lock_guard lock(m_mutex);
                m_requests.push_back(path);
                response = m_handler(path, ++m_counts[path]);
            }
            std::this_thread::sleep_for(response.delay);
            const auto header = "HTTP/1.1 " + std::to_string(response.status) + " Test\r\nContent-Length: " + std::to_string(response.body.size())
                + "\r\nConnection: close\r\n" + response.headers + "\r\n";
            const auto send_all = [&](const void* data, std::size_t size) {
                const auto* bytes = static_cast<const char*>(data);
                while (size) {
                    const auto count = ::send(client, bytes, size, MSG_NOSIGNAL);
                    if (count <= 0) {
                        break;
                    }
                    bytes += count;
                    size -= std::size_t(count);
                }
            };
            send_all(header.data(), header.size());
            send_all(response.body.data(), response.body.size());
            ::close(client);
        }
    }
    Handler m_handler;
    int m_socket = -1;
    unsigned m_port = 0;
    mutable std::mutex m_mutex;
    std::vector<std::string> m_requests;
    std::map<std::string, unsigned> m_counts;
    std::atomic<bool> m_stopped = false;
    std::vector<std::thread> m_threads;
};
} // namespace rf_test

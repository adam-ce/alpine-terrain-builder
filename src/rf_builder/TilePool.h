#pragma once

#include "Error.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <radix/tile.h>
#include <thread>
#include <utility>
#include <vector>

namespace rf_builder {

// RF's internal scheduling module. The coordinator alone submits/takes results.
// Fixed slots bound queued + active + completed tiles, not merely the job queue.
template <typename Payload>
class TilePool {
public:
    using Key = radix::tile::Id;
    using Prepare = std::function<Expected<Payload>(unsigned, const Key&)>;
    struct Completed {
        Key key;
        Expected<Payload> result;
    };

    TilePool(unsigned jobs, Prepare prepare)
        : m_prepare(std::move(prepare))
        , m_slots(std::size_t(jobs) * 2)
    {
        m_threads.reserve(jobs);
        for (unsigned worker = 0; worker < jobs; ++worker) {
            m_threads.emplace_back([this, worker](std::stop_token stop) { run(worker, stop); });
        }
    }
    ~TilePool() { join(); }
    TilePool(const TilePool&) = delete;
    TilePool& operator=(const TilePool&) = delete;

    bool submit(const Key& key)
    {
        std::lock_guard lock(m_mutex);
        if (m_stopped) {
            return false;
        }
        for (auto& slot : m_slots) {
            if (slot.state != State::Free) {
                continue;
            }
            slot.key = key;
            slot.sequence = m_sequence++;
            slot.state = State::Pending;
            ++m_outstanding;
            m_changed.notify_all();
            return true;
        }
        return false;
    }

    std::optional<Completed> take(std::chrono::milliseconds timeout)
    {
        std::unique_lock lock(m_mutex);
        m_changed.wait_for(lock, timeout, [&] { return ready() || m_outstanding == 0; });
        for (auto& slot : m_slots) {
            if (slot.state != State::Ready) {
                continue;
            }
            Completed result { slot.key, std::move(*slot.result) };
            slot.result.reset();
            slot.state = State::Free;
            --m_outstanding;
            return result;
        }
        return std::nullopt;
    }

    void stop()
    {
        std::lock_guard lock(m_mutex);
        stop_locked();
    }
    void join()
    {
        stop();
        m_threads.clear();
    }
    std::size_t outstanding() const
    {
        std::lock_guard lock(m_mutex);
        return m_outstanding;
    }
    std::optional<Error> failure() const
    {
        std::lock_guard lock(m_mutex);
        return m_failure;
    }

private:
    enum class State { Free, Pending, Active, Ready };
    struct Slot {
        State state = State::Free;
        Key key {};
        std::uint64_t sequence = 0;
        std::optional<Expected<Payload>> result = std::nullopt;
    };
    bool ready() const
    {
        return std::ranges::any_of(m_slots, [](const auto& slot) { return slot.state == State::Ready; });
    }
    std::optional<std::size_t> pending() const
    {
        std::optional<std::size_t> first = std::nullopt;
        for (std::size_t i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].state == State::Pending && (!first || m_slots[i].sequence < m_slots[*first].sequence)) {
                first = i;
            }
        }
        return first;
    }
    void stop_locked()
    {
        m_stopped = true;
        for (auto& slot : m_slots) {
            if (slot.state == State::Pending) {
                slot.state = State::Free;
                --m_outstanding;
            }
        }
        m_changed.notify_all();
    }
    void run(unsigned worker, std::stop_token stop)
    {
        for (;;) {
            std::unique_lock lock(m_mutex);
            m_changed.wait(lock, stop, [&] { return m_stopped || pending().has_value(); });
            if (m_stopped || stop.stop_requested()) {
                return;
            }
            auto& slot = m_slots[*pending()];
            slot.state = State::Active;
            const auto key = slot.key;
            lock.unlock();
            auto result = [&]() -> Expected<Payload> {
                try {
                    return m_prepare(worker, key);
                } catch (const std::exception& error) {
                    return Error::fail(Error::Code::Internal, "prepare RF tile " + to_string(key) + ": " + error.what());
                } catch (...) {
                    return Error::fail(Error::Code::Internal, "unknown exception preparing RF tile " + to_string(key));
                }
            }();
            lock.lock();
            if (!result && !m_failure) {
                m_failure = result.error();
                stop_locked();
            }
            slot.result.emplace(std::move(result));
            slot.state = State::Ready;
            m_changed.notify_all();
        }
    }
    Prepare m_prepare;
    std::vector<Slot> m_slots;
    mutable std::mutex m_mutex;
    std::condition_variable_any m_changed;
    std::uint64_t m_sequence = 0;
    std::size_t m_outstanding = 0;
    bool m_stopped = false;
    std::optional<Error> m_failure = std::nullopt;
    // Last: joins before any state used by workers is destroyed, including if
    // thread creation throws halfway through construction.
    std::vector<std::jthread> m_threads;
};
} // namespace rf_builder

#pragma once

#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <utility>

#include <expected>

namespace store {

template <typename Storage>
class ThreadSafeStorage {
    static_assert(!std::is_const_v<Storage>, "ThreadSafeStorage requires a non-const Storage");

public:
    using value_type = typename Storage::value_type;
    using key_type = typename Storage::key_type;
    using load_error = typename Storage::load_error;
    using save_error = typename Storage::save_error;

    explicit ThreadSafeStorage(Storage&& storage)
        : m_storage(std::move(storage))
    {
    }

    ThreadSafeStorage(const ThreadSafeStorage&) = delete;
    ThreadSafeStorage& operator=(const ThreadSafeStorage&) = delete;
    ThreadSafeStorage(ThreadSafeStorage&&) = delete;
    ThreadSafeStorage& operator=(ThreadSafeStorage&&) = delete;

    Storage release() && { return std::move(m_storage); }

    std::expected<value_type, load_error> load(const key_type& key) const
    {
        std::shared_lock lock(m_mutex);
        return m_storage.load(key);
    }

    auto has(const key_type& key) const
    {
        std::shared_lock lock(m_mutex);
        return m_storage.has(key);
    }

    std::filesystem::path base_path() const noexcept { return m_storage.base_path(); }

    std::expected<void, save_error> save(const key_type& key, const value_type& value) const
    {
        std::unique_lock lock(m_mutex);
        return m_storage.save(key, value);
    }

    auto save_or_create_index() const
    {
        std::unique_lock lock(m_mutex);
        return m_storage.save_or_create_index();
    }

private:
    mutable Storage m_storage;
    mutable std::shared_mutex m_mutex;
};

} // namespace store

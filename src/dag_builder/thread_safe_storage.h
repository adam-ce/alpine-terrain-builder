#pragma once

#include <filesystem>
#include <shared_mutex>
#include <type_traits>

#include <tl/expected.hpp>

#include "octree/Id.h"

namespace dag {

// A mutex-based thread-safe wrapper around a Storage_ instance.
// TODO: move this locking into Storage_ itself and remove this wrapper once that lands.
template <typename Storage>
class ThreadSafeStorage {
    static_assert(!std::is_const_v<Storage>, "ThreadSafeStorage requires a non-const Storage");

public:
    using value_type = typename Storage::value_type;
    using load_error = typename Storage::load_error;
    using save_error = typename Storage::save_error;

    explicit ThreadSafeStorage(Storage &&storage)
        : _storage(std::move(storage)) {}

    ThreadSafeStorage(const ThreadSafeStorage &) = delete;
    ThreadSafeStorage &operator=(const ThreadSafeStorage &) = delete;
    ThreadSafeStorage(ThreadSafeStorage &&) = delete;
    ThreadSafeStorage &operator=(ThreadSafeStorage &&) = delete;

    Storage release() && {
        return std::move(this->_storage);
    }

    tl::expected<value_type, load_error> load(const octree::Id &id) const {
        std::shared_lock lock(this->_mutex);
        return this->_storage.load(id);
    }

    bool has(const octree::Id &id) const {
        std::shared_lock lock(this->_mutex);
        return this->_storage.has(id);
    }

    std::filesystem::path base_path() const noexcept {
        return this->_storage.base_path();
    }

    tl::expected<void, save_error> save(const octree::Id &id, const value_type &value) const {
        std::unique_lock lock(this->_mutex);
        return this->_storage.save(id, value);
    }

    auto save_or_create_index() const {
        std::unique_lock lock(this->_mutex);
        return this->_storage.save_or_create_index();
    }

private:
    mutable Storage _storage;
    mutable std::shared_mutex _mutex;
};

} // namespace dag

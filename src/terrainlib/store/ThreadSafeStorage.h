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

    explicit ThreadSafeStorage(Storage&& storage)
        : m_storage(std::move(storage))
    {
    }

    ThreadSafeStorage(const ThreadSafeStorage&) = delete;
    ThreadSafeStorage& operator=(const ThreadSafeStorage&) = delete;
    ThreadSafeStorage(ThreadSafeStorage&&) = delete;
    ThreadSafeStorage& operator=(ThreadSafeStorage&&) = delete;

    Storage release() && { return std::move(m_storage); }

    auto load(const key_type& key) const
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

    auto save(const key_type& key, const value_type& value) const
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

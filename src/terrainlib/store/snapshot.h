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

#include <chrono>
#include <filesystem>
#include <vector>

#include <expected>

#include "Error.h"

// A snapshot is produced in an incomplete attempt <final>.<UTC time yyyymmddThhmmss.mmmZ>.part
// and renamed to <final> once complete, so an existing <final> is always complete. Each run
// creates its own attempt and leaves earlier ones unchanged, so that it can read and link
// their nodes; publication deletes them. An incomplete snapshot can serve as a cache.
// A run holds an exclusive lock on <final>.lock from creating its attempt until publication,
// so that a concurrent run on the same output fails instead of losing its attempt.
namespace store::snapshot {

inline constexpr auto checkpoint_interval = std::chrono::minutes(2);

struct Attempt;

// An exclusive flock on <final>.lock. Destruction, also by process exit, releases it.
// A default-constructed or moved-from lock holds nothing.
class Lock {
public:
    Lock() = default;
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
    Lock(Lock&& other) noexcept;
    Lock& operator=(Lock&& other) noexcept;
    ~Lock();

    bool held() const { return m_descriptor >= 0; }

private:
    friend Expected<Attempt> create_partial(const std::filesystem::path& final_path);
    explicit Lock(int descriptor);
    void unlock() noexcept;

    int m_descriptor = -1;
};

struct Attempt {
    std::filesystem::path path;
    Lock lock;
};

// Lexically normal and without a trailing separator, so that "out/" names the snapshot "out".
std::filesystem::path normalized(const std::filesystem::path& path);
bool is_partial(const std::filesystem::path& path);
Expected<void> require_absent(const std::filesystem::path& path);
// Requires that <final> does not exist, takes the lock of <final>, then creates a new attempt.
// Fails with AlreadyExists if another run holds the lock or an attempt with the same time exists.
Expected<Attempt> create_partial(const std::filesystem::path& final_path);
// The attempts of <final>, oldest first.
Expected<std::vector<std::filesystem::path>> attempts(const std::filesystem::path& final_path);
// Renames an attempt to its <final>, removes the input record and deletes the other
// attempts of <final>, then releases the lock of <final> from create_partial. The storage
// in the attempt must have saved its index and must not be used afterwards.
Expected<void> publish(const std::filesystem::path& partial_path, Lock lock);
// Hard links require the source and the output to be on the same filesystem.
Expected<void> check_link_filesystem(const std::filesystem::path& source, const std::filesystem::path& output);

} // namespace store::snapshot

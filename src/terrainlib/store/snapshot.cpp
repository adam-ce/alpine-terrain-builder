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

#include "store/snapshot.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <fmt/chrono.h>
#include <libassert/assert.hpp>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cli_support/log.h"
#include "io/utils.h"
#include "store/inputs.h"

namespace store::snapshot {

namespace {

    constexpr std::string_view partial_extension = ".part";
    // An attempt's UTC creation time; 0 stands for a digit.
    constexpr std::string_view time_pattern = "00000000T000000.000Z";

    // The final snapshot name of an attempt's directory name, if it is one.
    std::optional<std::string> final_name(const std::string& name)
    {
        const auto suffix_size = 1 + time_pattern.size() + partial_extension.size();
        if (name.size() <= suffix_size || !name.ends_with(partial_extension) || name[name.size() - suffix_size] != '.') {
            return std::nullopt;
        }
        const auto time = std::string_view(name).substr(name.size() - suffix_size + 1, time_pattern.size());
        for (std::size_t i = 0; i < time.size(); ++i) {
            if (time_pattern[i] == '0' ? std::isdigit(static_cast<unsigned char>(time[i])) == 0 : time[i] != time_pattern[i]) {
                return std::nullopt;
            }
        }
        return name.substr(0, name.size() - suffix_size);
    }

    std::filesystem::path lock_path(const std::filesystem::path& destination)
    {
        auto path = destination;
        path += ".lock";
        return path;
    }

} // namespace

Lock::Lock(const int descriptor)
    : m_descriptor(descriptor)
{
}

Lock::Lock(Lock&& other) noexcept
    : m_descriptor(std::exchange(other.m_descriptor, -1))
{
}

Lock& Lock::operator=(Lock&& other) noexcept
{
    if (this != &other) {
        unlock();
        m_descriptor = std::exchange(other.m_descriptor, -1);
    }
    return *this;
}

Lock::~Lock() { unlock(); }

void Lock::unlock() noexcept
{
    if (m_descriptor >= 0) {
        ::close(m_descriptor);
    }
    m_descriptor = -1;
}

std::filesystem::path normalized(const std::filesystem::path& path)
{
    auto result = path.lexically_normal();
    if (!result.has_filename() && result.has_relative_path()) {
        result = result.parent_path();
    }
    return result;
}

bool is_partial(const std::filesystem::path& path) { return normalized(path).extension() == ".part"; }

Expected<void> require_absent(const std::filesystem::path& path)
{
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error && error != std::errc::no_such_file_or_directory) {
        return Error::fail(Error::Code::Io, "inspect snapshot destination", path, error);
    }
    if (std::filesystem::exists(status)) {
        return Error::fail(Error::Code::AlreadyExists, "create snapshot at", path);
    }
    return {};
}

Expected<Attempt> create_partial(const std::filesystem::path& final_path)
{
    const auto destination = normalized(final_path);
    if (destination.filename().empty() || destination.filename() == "." || destination.filename() == ".." || is_partial(destination)) {
        return Error::fail(Error::Code::InvalidInput, "creation requires a final snapshot path without a .part suffix", final_path);
    }
    // Checked before any side effect, so that no lock file appears beside a published snapshot.
    if (auto absent = require_absent(destination); !absent) {
        return Error::propagate(std::move(absent));
    }
    std::error_code error;
    if (!destination.parent_path().empty()) {
        std::filesystem::create_directories(destination.parent_path(), error);
        if (error) {
            return Error::fail(Error::Code::Io, "create snapshot parent directory", destination.parent_path(), error);
        }
    }
    const auto path = lock_path(destination);
    const int descriptor = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (descriptor < 0) {
        return Error::fail(Error::Code::Io, "open snapshot lock", path, std::error_code(errno, std::generic_category()));
    }
    Lock lock(descriptor);
    if (::flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
        const auto lock_error = std::error_code(errno, std::generic_category());
        if (lock_error == std::errc::operation_would_block) {
            return Error::fail(Error::Code::AlreadyExists, "another run is building", destination);
        }
        return Error::fail(Error::Code::Io, "lock snapshot", path, lock_error);
    }
    // The previous holder may have published in the meantime.
    if (auto absent = require_absent(destination); !absent) {
        return Error::propagate(std::move(absent));
    }
    // Names are distinct per millisecond and the lock excludes concurrent runs, so creating
    // attempts of one output within a millisecond fails with AlreadyExists.
    const auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
    const auto seconds = std::chrono::floor<std::chrono::seconds>(now);
    auto partial = destination;
    partial += fmt::format(".{:%Y%m%dT%H%M%S}.{:03}Z{}", seconds, (now - seconds).count(), partial_extension);
    if (auto absent = require_absent(partial); !absent) {
        return Error::propagate(std::move(absent));
    }
    if (!std::filesystem::create_directory(partial, error)) {
        return Error::fail(error ? Error::Code::Io : Error::Code::AlreadyExists, "create incomplete snapshot", partial, error);
    }
    return Attempt { std::move(partial), std::move(lock) };
}

Expected<std::vector<std::filesystem::path>> attempts(const std::filesystem::path& final_path)
{
    const auto destination = normalized(final_path);
    const auto directory = destination.parent_path().empty() ? std::filesystem::path(".") : destination.parent_path();
    std::vector<std::filesystem::path> result;
    std::error_code error;
    std::filesystem::directory_iterator entry(directory, error);
    if (error == std::errc::no_such_file_or_directory) {
        return result;
    }
    for (; !error && entry != std::filesystem::directory_iterator(); entry.increment(error)) {
        const auto name = entry->path().filename().string();
        // Entries that cannot be inspected, such as dangling symlinks, are not attempts.
        std::error_code type_error;
        if (final_name(name) == destination.filename().string() && entry->is_directory(type_error)) {
            result.push_back(destination.parent_path() / name);
        }
    }
    if (error) {
        return Error::fail(Error::Code::Io, "list incomplete snapshot attempts", directory, error);
    }
    // The fixed-width times sort chronologically.
    std::ranges::sort(result);
    return result;
}

Expected<void> publish(const std::filesystem::path& partial_path, const Lock lock)
{
    const auto partial = normalized(partial_path);
    const auto name = final_name(partial.filename().string());
    if (!name) {
        return Error::fail(Error::Code::InvalidInput, "publication requires an incomplete snapshot attempt <final>.<time>.part", partial_path);
    }
    const auto destination = partial.parent_path() / *name;
    ASSERT(lock.held(), "publication requires the lock of its snapshot from create_partial");
    if (auto absent = require_absent(destination); !absent) {
        return absent;
    }
    // The record stays until the rename succeeded, so a .part that failed to publish remains a cache.
    auto renamed = ::io::utils::rename_without_replacement(partial, destination);
    if (!renamed) {
        return Error::propagate(std::move(renamed), "publish snapshot");
    }
    // A leftover record is harmless, as a published snapshot is never accepted as a cache.
    const auto inputs_path = destination / inputs::file_name;
    std::error_code error;
    std::filesystem::remove(inputs_path, error);
    if (error) {
        LOG_WARN("Failed to remove the input record {} from the published snapshot: {}", inputs_path, error.message());
    }
    // Safe while held: any run that locks afterwards, this file or a new one, finds <final> and fails.
    const auto lock_file = lock_path(destination);
    std::filesystem::remove(lock_file, error);
    if (error) {
        LOG_WARN("Failed to remove the lock {} of the published snapshot: {}", lock_file, error.message());
    }
    // Earlier attempts were reused or are obsolete; failing to delete them leaves the publication intact.
    const auto others = attempts(destination);
    if (!others) {
        LOG_WARN("Failed to list the earlier attempts of {}: {}", destination, others.error().to_string());
        return {};
    }
    for (const auto& attempt : *others) {
        std::filesystem::remove_all(attempt, error);
        if (error) {
            LOG_WARN("Failed to delete the earlier attempt {}: {}", attempt, error.message());
        } else {
            LOG_INFO("Deleted earlier attempt {}", attempt);
        }
    }
    return {};
}

Expected<void> check_link_filesystem(const std::filesystem::path& source, const std::filesystem::path& output)
{
    struct stat source_status {};
    if (::stat(source.c_str(), &source_status) != 0) {
        return Error::fail(Error::Code::Io, "inspect link source filesystem", source, std::error_code(errno, std::generic_category()));
    }
    auto parent = std::filesystem::absolute(output).parent_path();
    struct stat output_status {};
    while (::stat(parent.c_str(), &output_status) != 0) {
        if (errno != ENOENT || parent == parent.parent_path()) {
            return Error::fail(Error::Code::Io, "inspect output filesystem", parent, std::error_code(errno, std::generic_category()));
        }
        parent = parent.parent_path();
    }
    if (source_status.st_dev != output_status.st_dev) {
        return Error::fail(Error::Code::Unsupported, "hard links require the source and the output to be on the same filesystem", source);
    }
    return {};
}

} // namespace store::snapshot

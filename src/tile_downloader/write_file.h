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
#include <fstream>
#include <stdexcept>
#include <vector>

#include <fmt/core.h>

namespace tile_downloader_detail {

inline void write_file_checked_direct(const std::filesystem::path& path, const std::vector<char>& data)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error(fmt::format("failed to open \"{}\" for writing", path.string()));
    }

    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!output) {
        throw std::runtime_error(fmt::format("failed to write \"{}\"", path.string()));
    }

    output.close();
    if (!output) {
        throw std::runtime_error(fmt::format("failed to finish writing \"{}\"", path.string()));
    }
}

} // namespace tile_downloader_detail

[[nodiscard]] inline std::filesystem::path partial_tile_path(const std::filesystem::path& path)
{
    auto partial_path = path;
    partial_path += ".part";
    return partial_path;
}

[[nodiscard]] inline std::filesystem::path children_pending_tile_path(const std::filesystem::path& path)
{
    auto pending_path = path;
    pending_path += ".children-pending";
    return pending_path;
}

inline void write_file_children_pending(const std::filesystem::path& path, const std::vector<char>& data)
{
    const auto partial_path = partial_tile_path(path);
    const auto pending_path = children_pending_tile_path(path);

    try {
        tile_downloader_detail::write_file_checked_direct(partial_path, data);
        std::filesystem::rename(partial_path, pending_path);
    } catch (...) {
        std::error_code cleanup_error;
        std::filesystem::remove(partial_path, cleanup_error);
        throw;
    }
}

inline void mark_tile_children_complete(const std::filesystem::path& path)
{
    const auto pending_path = children_pending_tile_path(path);
    std::filesystem::rename(pending_path, path);
}

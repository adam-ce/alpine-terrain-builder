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

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "Error.h"

namespace raster_store::attribution {

inline constexpr std::string_view file_name = "source_attribution_table.json";
inline constexpr std::uint32_t index_limit = 65535;

struct Entity {
    double spatial_resolution;
    std::string acquisition_date;
    std::string ingestion_date;
    std::string copyright;
    std::string copyright_link;
    std::string license;

    bool operator==(const Entity&) const = default;
};

struct Table {
    std::vector<Entity> entities;

    Expected<const Entity*> at(std::uint32_t index) const;
};

Expected<Table> parse(std::string_view json);
Expected<std::filesystem::path> find_table(const std::filesystem::path& index_path);
Expected<Table> read_table(const std::filesystem::path& index_path);
Expected<void> copy_table(const std::filesystem::path& source_index_path, const std::filesystem::path& destination_directory);

} // namespace raster_store::attribution

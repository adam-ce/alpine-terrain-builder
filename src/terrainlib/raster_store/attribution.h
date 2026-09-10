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

#pragma once

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <fmt/core.h>

namespace tile_downloader_detail {

inline void write_file_checked_direct(const std::filesystem::path &path, const std::vector<char> &data)
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

}

inline void write_file_checked(const std::filesystem::path &path, const std::vector<char> &data)
{
    auto staging_path = path;
    staging_path += ".part";

    try {
        tile_downloader_detail::write_file_checked_direct(staging_path, data);
        std::filesystem::rename(staging_path, path);
    } catch (...) {
        std::error_code cleanup_error;
        std::filesystem::remove(staging_path, cleanup_error);
        throw;
    }
}

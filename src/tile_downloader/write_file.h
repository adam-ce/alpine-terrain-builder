#pragma once

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <fmt/core.h>

inline void write_file_checked(const std::filesystem::path &path, const std::vector<char> &data)
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

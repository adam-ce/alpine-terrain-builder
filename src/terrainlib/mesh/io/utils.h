#pragma once

#include <filesystem>

namespace mesh {
namespace io {
namespace utils {

std::filesystem::path create_parent_directories(const std::filesystem::path &path);

} // namespace utils
} // namespace io
} // namespace mesh

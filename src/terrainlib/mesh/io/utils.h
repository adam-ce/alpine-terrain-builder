#pragma once

#include <filesystem>

namespace mesh::io::utils {

std::filesystem::path create_parent_directories(const std::filesystem::path &path);

} // namespace mesh::io::utils

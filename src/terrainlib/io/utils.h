#pragma once

#include <filesystem>

namespace io::utils {

std::filesystem::path create_parent_directories(const std::filesystem::path &path);

} // namespace io::utils

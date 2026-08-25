#pragma once

#include <filesystem>
#include <system_error>

namespace io::utils {

[[nodiscard]] std::error_code create_parent_directories(const std::filesystem::path &path);

} // namespace io::utils

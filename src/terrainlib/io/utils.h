#pragma once

#include <filesystem>
#include <system_error>

#include "Error.h"

namespace io::utils {

[[nodiscard]] Expected<void> create_parent_directories(const std::filesystem::path& path);
Expected<void> rename_without_replacement(const std::filesystem::path& source, const std::filesystem::path& destination);

} // namespace io::utils

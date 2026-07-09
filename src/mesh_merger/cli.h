#pragma once

#include <filesystem>
#include <vector>
#include <optional>

#include <spdlog/spdlog.h>

namespace cli {

struct Args {
    std::vector<std::filesystem::path> mesh_paths;
    std::filesystem::path output_path;
    std::optional<double> epsilon;
    spdlog::level::level_enum log_level;
};

Args parse(int argc, const char *const *argv);

}

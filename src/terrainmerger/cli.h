#pragma once

#include <vector>
#include <filesystem>

#include <spdlog/spdlog.h>

namespace cli {

struct Args {
    spdlog::level::level_enum log_level;
    std::vector<std::filesystem::path> input_paths;
    std::filesystem::path output_path;
};

Args parse(int argc, const char *const *argv);

}

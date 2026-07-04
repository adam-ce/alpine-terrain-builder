#pragma once

#include <filesystem>

#include <spdlog/spdlog.h>

namespace cli {

struct Args {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    spdlog::level::level_enum log_level;
};

Args parse(int argc, const char *const *argv);

}

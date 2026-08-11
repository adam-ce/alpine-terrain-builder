#pragma once

#include <filesystem>

#include <spdlog/spdlog.h>

namespace cli {

struct Args {
    std::filesystem::path dataset_path;
    bool full_view;
    spdlog::level::level_enum log_level;
};

Args parse(int argc, const char *const *argv);

}

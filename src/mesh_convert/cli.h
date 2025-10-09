#pragma once

#include <filesystem>
#include <optional>

#include <spdlog/spdlog.h>
#include <glm/glm.hpp>

namespace cli {
    struct Args {
        std::filesystem::path input_path;
        std::filesystem::path output_path;
        spdlog::level::level_enum log_level;
        std::optional<glm::uvec2> texture_resolution;
    };

    Args parse(int argc, const char *const *argv);
}


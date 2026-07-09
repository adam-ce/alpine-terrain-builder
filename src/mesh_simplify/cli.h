#ifndef CLI_H
#define CLI_H

#include "pch.h"

#include <spdlog/spdlog.h>

#include "simplify.h"

namespace cli {
    struct SimplificationArgs {
        std::vector<simplify::StopCondition> stop_condition;
    };

    struct Args {
        std::filesystem::path output_path;
        std::vector<std::filesystem::path> input_paths;
        std::optional<SimplificationArgs> simplification;
        spdlog::level::level_enum log_level;
        bool save_intermediate_meshes;
        std::optional<glm::uvec2> target_texture_resolution;
    };

    Args parse(int argc, const char *const *argv);

    namespace {
    template <typename T, std::size_t Extent>
    Args _parse(const std::span<const T, Extent> args) {
        std::array<const char *, Extent> raw_args;
        std::transform(args.begin(), args.end(), raw_args.begin(),
                       [](const T &str) { return str.data(); });
        return cli::parse(raw_args.size(), raw_args.data());
    }
    template <typename T>
    Args _parse(const std::span<const T, std::dynamic_extent> args) {
        std::vector<const char *> raw_args;
        raw_args.resize(args.size());
        std::transform(args.begin(), args.end(), raw_args.begin(),
                        [](const T &str) { return str.data(); });
        return cli::parse(raw_args.size(), raw_args.data());
    }
    }

    template <std::size_t Extent>
    Args cli::parse(const std::span<const std::string, Extent> args) {
        return _parse(args);
    }
    template <std::size_t Extent>
    Args cli::parse(const std::span<const std::string_view, Extent> args) {
        return _parse(args);
    }
}

#endif

#pragma once

#include <vector>
#include <filesystem>

#include <spdlog/spdlog.h>

namespace cli {

struct BaseArgs {
    spdlog::level::level_enum log_level;
};

struct MergeArgs : public BaseArgs {
    std::filesystem::path base_path;
    std::filesystem::path new_path;
    std::optional<std::filesystem::path> mask_path;
    std::optional<std::filesystem::path> output_path;
};

struct CutArgs : public BaseArgs {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    std::filesystem::path mask_path;
};

using Args = std::variant<MergeArgs, CutArgs>;

Args parse(int argc, const char *const *argv);

}

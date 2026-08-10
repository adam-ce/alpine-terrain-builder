#pragma once

#include <vector>
#include <cstdint>
#include <optional>
#include <filesystem>

#include <spdlog/spdlog.h>

#include "build.h"
#include "octree/Id.h"
#include "Range.h"

namespace cli {

enum class TextureSizingKind {
    Constant,
    Relative,
};

struct Args {
    spdlog::level::level_enum log_level;

    std::filesystem::path input_path;
    std::filesystem::path output_path;
    octree::Id root_node;
    AnyRange<uint32_t> level_range;

    bool allow_texture_reuse;
    ChartingMode charting;
    uint32_t clusters_per_partition;
    std::optional<float> target_ratio;
    std::optional<float> target_error;

    TextureSizingOptions sizing_options;
    uint32_t texture_gutter;

    bool write_debug_meshes;
    bool parallelize;
    dag::IncludeMode include_mode;
    ContinuationMode continuation_mode;
};

Args parse(int argc, const char *const *argv);

}

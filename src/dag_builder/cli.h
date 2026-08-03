#pragma once

#include <vector>
#include <cstdint>
#include <optional>
#include <filesystem>

#include <spdlog/spdlog.h>

#include "build.h"
#include "octree/Id.h"
#include "Range.h"
#include "uv/unwrap.h"

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

    uv::Algorithm uv_unwrap_algorithm;
    bool allow_texture_reuse;
    uint32_t clusters_per_partition;
    std::optional<float> target_ratio;
    std::optional<float> target_error;

    TextureSizingKind texture_sizing_kind;
    uint32_t texels_per_cluster;
    uint32_t min_cluster_texture_size;
    uint32_t max_cluster_texture_size;

    bool write_debug_meshes;
    bool parallelize;
    dag::IncludeMode include_mode;
    ContinuationMode continuation_mode;
};

Args parse(int argc, const char *const *argv);

}

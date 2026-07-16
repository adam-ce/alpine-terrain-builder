#pragma once

#include <vector>
#include <cstdint>
#include <optional>
#include <filesystem>

#include <spdlog/spdlog.h>

#include "octree/Id.h"
#include "Range.h"
#include "uv/unwrap.h"

namespace cli {

struct Args {
    spdlog::level::level_enum log_level;

    std::filesystem::path input_path;
    std::filesystem::path output_path;
    octree::Id root_node;
    AnyRange<uint32_t> level_range;

    uv::Algorithm uv_unwrap_algorithm;
    uint32_t clusters_per_partition;
    std::optional<float> target_ratio;
    std::optional<float> target_error;

    bool write_debug_meshes;
    bool overwrite;
};

Args parse(int argc, const char *const *argv);

}

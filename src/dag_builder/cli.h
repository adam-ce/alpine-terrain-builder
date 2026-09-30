/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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

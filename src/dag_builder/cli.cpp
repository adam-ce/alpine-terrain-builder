/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
 * Copyright (C) 2026 Adam Celarek-Litofcenko
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

#include "cli.h"

#include <CLI/CLI.hpp>
#include <libassert/assert.hpp>
#include <spdlog/spdlog.h>

#include "cli_support/log.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace cli;

namespace {

const std::unordered_map<std::string, dag::IncludeMode> include_mode_names{
    {"current", dag::IncludeMode::CurrentOnly},
    {"current-and-coarser", dag::IncludeMode::CurrentAndCoarser},
};

const std::unordered_map<std::string, ChartingMode> charting_mode_names{
    {"per-cluster", ChartingMode::PerCluster},
    {"per-node", ChartingMode::PerNode},
};

// Use RelativeQuality texture budgeting mode.
const std::string relative_texels_per_triangle = "relative";

std::variant<ConstantQuality, RelativeQuality> make_texture_sizing_mode(const std::string &texels_per_triangle) {
    if (texels_per_triangle == relative_texels_per_triangle) {
        return RelativeQuality{};
    }
    return ConstantQuality{std::stod(texels_per_triangle)};
}


AnyRange<uint32_t> make_level_range(const std::vector<uint32_t>& input) {
    switch (input.size()) {
        case 0:
            return RangeFull{};
        case 1:
            return RangeInclusive<uint32_t>(input[0]);
        case 2: {
            const uint32_t min_level = input[0];
            const uint32_t max_level = input[1];
            if (min_level > max_level) {
                throw CLI::ValidationError("--levels minimum must be <= maximum");
            }
            return RangeInclusive<uint32_t>{min_level, max_level};
        }
        default:
            UNREACHABLE();
    }
}

octree::Id make_octree_id(const std::vector<uint64_t> &values) {
    if (values.size() == 2) {
        const octree::Id::Level level = values[0];
        const octree::Id::Index index = values[1];

        return octree::Id(level, index);
    }

    if (values.size() == 4) {
        const octree::Id::Level level = values[0];
        const octree::Id::Coord x = values[1];
        const octree::Id::Coord y = values[2];
        const octree::Id::Coord z = values[3];

        return octree::Id(level, x, y, z);
    }

    throw CLI::ValidationError("--root-node expects either: <level> <index> or <level> <x> <y> <z>");
}

} // namespace

Args cli::parse(int argc, const char *const *argv) {
    DEBUG_ASSERT(argc >= 0);

    CLI::App app{"dag_builder"};
    app.allow_windows_style_options();

    Args args{
        .log_level = spdlog::level::level_enum::info,
        .input_path = {},
        .output_path = {},
        .root_node = octree::Id::root(),
        .level_range = RangeFull{},
        .allow_texture_reuse = true,
        .charting = ChartingMode::PerCluster,
        .clusters_per_partition = 8,
        .target_ratio = std::nullopt,
        .target_error = std::nullopt,
        .sizing_options = {},
        .texture_gutter = 1,
        .write_debug_meshes = false,
        .parallelize = false,
        .include_mode = dag::IncludeMode::CurrentOnly,
        .cache_path = std::nullopt
    };

    app.add_option("--input", args.input_path, "Path to input mesh dataset")
        ->required()
        ->check(CLI::ExistingDirectory);

    app.add_option("--output",
           args.output_path,
           "New path for the generated dag levels; they are written to <output>.<UTC time>.part, which is renamed to <output> when complete")
        ->required();

    std::vector<uint64_t> root_node_values;
    app.add_option(
           "--root-node",
           root_node_values,
           "Root octree node as either: <level> <index> or <level> <x> <y> <z>")
        ->expected(2, 4)
        ->check(CLI::NonNegativeNumber);

    std::vector<uint32_t> level_range_values;
    app.add_option("--levels", level_range_values, "Inclusive level range to generate: <level> or <min> <max>")
        ->expected(1, 2)
        ->check(CLI::NonNegativeNumber);


    app.add_flag("!--no-texture-reuse", args.allow_texture_reuse, "Always unwrap merged clusters instead of adopting a shared source texture");

    app.add_option("--charting", args.charting, "How the clusters of a node are unwrapped")
        ->transform(CLI::CheckedTransformer(charting_mode_names, CLI::ignore_case))
        ->default_val(args.charting);

    app.add_option("--clusters-per-group", args.clusters_per_partition, "Target number of clusters per partition")
        ->default_val(args.clusters_per_partition)
        ->check(CLI::PositiveNumber);

    app.add_option("--target-ratio", args.target_ratio, "Simplification target ratio")
        ->check(CLI::Range(0.0, 1.0));

    app.add_option("--target-error", args.target_error, "Simplification target error as a fraction of node bounds")
        ->check(CLI::NonNegativeNumber);

    std::string texels_per_triangle = std::to_string(ConstantQuality{}.target_texels_per_triangle);

    app.add_option("--texels-per-triangle", texels_per_triangle, "Texel budget of a triangle, or relative to follow the source textures")
        ->default_str("64")
        ->check(CLI::PositiveNumber | CLI::IsMember({relative_texels_per_triangle}));

    app.add_option("--max-node-texels", args.sizing_options.max_node_texels, "Texel budget shared by all textures of a node")
        ->default_val(args.sizing_options.max_node_texels)
        ->check(CLI::PositiveNumber);

    app.add_option("--texture-gutter", args.texture_gutter, "Texels kept between charts.")
        ->default_val(args.texture_gutter)
        ->check(CLI::NonNegativeNumber);

    app.add_option("--cache", args.cache_path, "Incomplete .part output of an earlier build with the same inputs and options to reuse")
        ->check(CLI::ExistingDirectory);

    app.add_flag("--write-debug-meshes", args.write_debug_meshes, "Write debug .glb meshes alongside the output");

    app.add_flag("--parallelize", args.parallelize, "Build nodes within a level in parallel");

    app.add_option("--include-mode", args.include_mode, "Which input nodes to include when building a level")
        ->transform(CLI::CheckedTransformer(include_mode_names, CLI::ignore_case))
        ->default_val(args.include_mode);

    app.add_option("--verbosity", args.log_level, "Verbosity level of logging")
        ->transform(CLI::CheckedTransformer(cli_support::log_level_names(), CLI::ignore_case))
        ->default_val(args.log_level);

    try {
        app.parse(argc, argv);

        args.level_range = make_level_range(level_range_values);
        if (!root_node_values.empty()) {
            args.root_node = make_octree_id(root_node_values);
        }

        args.sizing_options.mode = make_texture_sizing_mode(texels_per_triangle);
    } catch (const CLI::ParseError &e) {
        std::exit(app.exit(e));
    }

    return args;
}

#include "cli.h"

#include <CLI/CLI.hpp>
#include <libassert/assert.hpp>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <unordered_map>
#include <optional>
#include <string>
#include <vector>

using namespace cli;

namespace {

const std::unordered_map<std::string, spdlog::level::level_enum> log_level_names{
    {"off", spdlog::level::level_enum::off},
    {"critical", spdlog::level::level_enum::critical},
    {"error", spdlog::level::level_enum::err},
    {"warn", spdlog::level::level_enum::warn},
    {"info", spdlog::level::level_enum::info},
    {"debug", spdlog::level::level_enum::debug},
    {"trace", spdlog::level::level_enum::trace},
};

const std::unordered_map<std::string, uv::Algorithm> uv_unwrap_algorithm_names{
    {"AsRigidAsPossible", uv::Algorithm::AsRigidAsPossible},
    {"arap", uv::Algorithm::AsRigidAsPossible},

    {"DiscreteAuthalic", uv::Algorithm::DiscreteAuthalic},
    {"da", uv::Algorithm::DiscreteAuthalic},

    {"DiscreteConformalMap", uv::Algorithm::DiscreteConformalMap},
    {"dcm", uv::Algorithm::DiscreteConformalMap},

    {"FloaterMeanValueCoordinates", uv::Algorithm::FloaterMeanValueCoordinates},
    {"fmvc", uv::Algorithm::FloaterMeanValueCoordinates},

    {"LeastSquaresConformalMap", uv::Algorithm::LeastSquaresConformalMap},
    {"lscm", uv::Algorithm::LeastSquaresConformalMap},

    {"TutteBarycentricMapping", uv::Algorithm::TutteBarycentricMapping},
    {"tutte", uv::Algorithm::TutteBarycentricMapping}};

Range<uint32_t> make_level_range(const std::vector<uint32_t>& input) {
    switch (input.size()) {
        case 0:
            return full_range<uint32_t>();
        case 1:
            return Range<uint32_t>(input[0]);
        case 2: {
            const uint32_t min_level = input[0];
            const uint32_t max_level = input[1];
            if (min_level > max_level) {
                throw CLI::ValidationError("--levels minimum must be <= maximum");
            }
            return Range<uint32_t>{min_level, max_level};
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
        .level_range = full_range<uint32_t>(),
        .uv_unwrap_algorithm = {},
        .clusters_per_partition = 8,
        .target_ratio = std::nullopt,
        .target_error = std::nullopt,
        .write_debug_meshes = false,
        .overwrite = false,
    };

    app.add_option("--input", args.input_path, "Path to input mesh dataset")
        ->required()
        ->check(CLI::ExistingDirectory);

    app.add_option("--output", args.output_path, "Path to write generated dag levels")
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

    app.add_option("--uv-unwrap-algorithm", args.uv_unwrap_algorithm, "UV unwrap algorithm")
        ->transform(CLI::CheckedTransformer(uv_unwrap_algorithm_names, CLI::ignore_case));

    app.add_option("--clusters-per-group", args.clusters_per_partition, "Target number of clusters per partition")
        ->default_val(args.clusters_per_partition)
        ->check(CLI::PositiveNumber);

    app.add_option("--target-ratio", args.target_ratio, "Simplification target ratio")
        ->check(CLI::Range(0.0, 1.0));

    app.add_option("--target-error", args.target_error, "Simplification target error as a fraction of node bounds")
        ->check(CLI::NonNegativeNumber);

    app.add_flag("--overwrite", args.overwrite, "Overwrite data already present in output");

    app.add_flag("--write-debug-meshes", args.write_debug_meshes, "Write debug .glb meshes alongside the output");

    app.add_option("--verbosity", args.log_level, "Verbosity level of logging")
        ->transform(CLI::CheckedTransformer(log_level_names, CLI::ignore_case))
        ->default_val(args.log_level);

    try {
        app.parse(argc, argv);

        args.level_range = make_level_range(level_range_values);
        if (!root_node_values.empty()) {
            args.root_node = make_octree_id(root_node_values);
        }
    } catch (const CLI::ParseError &e) {
        std::exit(app.exit(e));
    }

    return args;
}

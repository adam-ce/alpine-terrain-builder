#include "cli.h"

#include <CLI/CLI.hpp>
#include <libassert/assert.hpp>
#include <spdlog/spdlog.h>

using namespace cli;

Args cli::parse(int argc, const char *const *argv) {
    DEBUG_ASSERT(argc >= 0);

    CLI::App app{"terrainmerger"};
    app.allow_windows_style_options();
    // argv = app.ensure_utf8(argv);

    auto &merge = *app.add_subcommand("merge");
    MergeArgs merge_args;
    merge.add_option("--base", merge_args.base_path, "Path to base dataset")
        ->required()
        ->check(CLI::ExistingDirectory);

    merge.add_option("--new", merge_args.new_path, "Path to new dataset to merge into base")
        ->required()
        ->check(CLI::ExistingDirectory);

    merge.add_option("--mask", merge_args.mask_path, "Path to a mask denoting the valid area of the new dataset")
        ->check(CLI::ExistingFile);

    merge.add_option("--output", merge_args.output_path, "Path to output write the merged dataset to")
        ->required();

    merge.add_option("--overwrite", merge_args.overwrite_output, "Overwrite any data already in output");

    /*
    const std::map<std::string, MergeAlgorithm> method_names{
        {"combine", MergeAlgorithm::Combine}, {"masked", MergeAlgorithm::Masked}, {"project", MergeAlgorithm::Project}};
    merge.add_option("--method", merge_args.algorihm, "Method to use for merging")
        ->transform(CLI::CheckedTransformer(method_names, CLI::ignore_case));
    */

    merge.fallthrough();

    auto &cut = *app.add_subcommand("cut");
    CutArgs cut_args;
    cut.add_option("--input", cut_args.input_path, "Path to dataset to cut")
        ->required()
        ->check(CLI::ExistingDirectory);

    cut.add_option("--mask", cut_args.mask_path, "Path to a mask denoting the regions to retain")
        ->check(CLI::ExistingFile);

    cut.add_option("--output", cut_args.output_path, "Path to output the cut dataset to");

    app.add_flag("--keep-inside,!--keep-outside", cut_args.keep_inside, "Keep the part of the dataset thats inside/outside the mask.");

    cut.fallthrough();

    spdlog::level::level_enum log_level;
    const std::map<std::string, spdlog::level::level_enum> log_level_names{
        {"off", spdlog::level::level_enum::off},
        {"critical", spdlog::level::level_enum::critical},
        {"error", spdlog::level::level_enum::err},
        {"warn", spdlog::level::level_enum::warn},
        {"info", spdlog::level::level_enum::info},
        {"debug", spdlog::level::level_enum::debug},
        {"trace", spdlog::level::level_enum::trace}};
    app.add_option("--verbosity", log_level, "Verbosity level of logging")
        ->transform(CLI::CheckedTransformer(log_level_names, CLI::ignore_case))
        ->default_val(spdlog::level::level_enum::info);

    app.require_subcommand(1);

    try {
        // app.parse(argc, argv);
        // app.parse("--input ../../../meshes/innenstadt2 ../../../meshes/vienna2 --output ../../../meshes/out --verbosity trace");
        app.parse("merge --new ../../../meshes/innenstadt --base ../../../meshes/vienna --mask ../../../meshes/mask.geojson --output ../../../meshes/out-merge --verbosity trace");
        // app.parse("cut --input ../../../meshes/innenstadt --mask ../../../meshes/mask.geojson --output ../../../meshes/out-cut --keep-outside --verbosity trace");
    } catch (const CLI::ParseError &e) {
        exit(app.exit(e));
    }

    Args args;
    if (merge) {
        merge_args.log_level = log_level;
        args = std::move(merge_args);
    } else if (cut) {
        cut_args.log_level = log_level;
        args = std::move(cut_args);
    }
    return args;
}

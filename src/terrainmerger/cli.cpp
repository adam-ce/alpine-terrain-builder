#include "cli.h"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <libassert/assert.hpp>

using namespace cli;

Args cli::parse(int argc, const char * const * argv) {
    DEBUG_ASSERT(argc >= 0);

    CLI::App app{"terrainmerger"};
    app.allow_windows_style_options();
    // argv = app.ensure_utf8(argv);

    auto& merge = *app.add_subcommand("merge");
    MergeArgs merge_args;
    merge.add_option("--base", merge_args.base_path, "Path to base dataset")
        ->required()
        ->check(CLI::ExistingDirectory);

    merge.add_option("--new", merge_args.new_path, "Path to new dataset to merge into base")
        ->required()
        ->check(CLI::ExistingDirectory);

    merge.add_option("--mask", merge_args.mask_path, "Path to a mask denoting the valid area of the new dataset")
        ->check(CLI::ExistingFile);

    merge.add_option("--output", merge_args.output_path, "Path to output write the merged dataset to (defaults to --base)");

    merge.fallthrough();

    auto& cut = *app.add_subcommand("cut");
    CutArgs cut_args;
    cut.add_option("--input", cut_args.input_path, "Path to dataset to cut")
        ->required()
        ->check(CLI::ExistingDirectory);

    cut.add_option("--mask", cut_args.mask_path, "Path to a mask denoting the regions to retain")
        ->check(CLI::ExistingFile);

    cut.add_option("--output", cut_args.output_path, "Path to output the cut dataset to");

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
        app.parse(argc, argv);
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

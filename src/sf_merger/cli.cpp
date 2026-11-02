/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Martin Braunsperger
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

using namespace cli;

Args cli::parse(int argc, const char *const *argv) {
    DEBUG_ASSERT(argc >= 0);

    CLI::App app{"mesh_merger"};
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

    merge
        .add_option("--output",
            merge_args.output_path,
            "New path for the merged dataset; it is written to <output>.<UTC time>.part, which is renamed to <output> when complete")
        ->required();

    merge.add_option("--cache", merge_args.cache_path, "Incomplete .part output of an earlier merge with the same inputs to reuse")
        ->check(CLI::ExistingDirectory);

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
        ->required()
        ->check(CLI::ExistingFile);

    cut.add_option("--output",
           cut_args.output_path,
           "New path for the cut dataset; it is written to <output>.<UTC time>.part, which is renamed to <output> when complete")
        ->required();

    cut.add_flag("--keep-inside,!--keep-outside", cut_args.keep_inside, "Keep the part of the dataset thats inside/outside the mask.");

    cut.fallthrough();

    spdlog::level::level_enum log_level;
    app.add_option("--verbosity", log_level, "Verbosity level of logging")
        ->transform(CLI::CheckedTransformer(cli_support::log_level_names(), CLI::ignore_case))
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

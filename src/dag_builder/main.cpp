/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 * Copyright (C) 2025 Adam Celarek-Litofcenko
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

#include <cstdlib>
#include <exception>
#include <filesystem>

#include "cli.h"
#include "build.h"
#include "log.h"
#include "mesh/storage.h"
#include "storage.h"
#include "ContinuationMode.h"

int main(int argc, char **argv) {
    const cli::Args args = cli::parse(argc, argv);

    Log::init(args.log_level);

    try {
        auto input_result = mesh::storage::open_folder_indexed(args.input_path);
        if (!input_result) {
            LOG_ERROR(
                "Failed to open input dataset {}: {}",
                args.input_path,
                input_result.error().to_string());
            return EXIT_FAILURE;
        }
        auto output_result = dag::storage::open_folder_indexed(args.output_path);
        if (!output_result) {
            LOG_ERROR(
                "Failed to open output dataset {}: {}",
                args.output_path,
                output_result.error().to_string());
            return EXIT_FAILURE;
        }
        const mesh::storage::IndexedStorage input_storage = std::move(input_result.value());
        dag::storage::IndexedStorage output_storage = std::move(output_result.value());
        output_storage.settings().allow_overwrite = args.continuation_mode == ContinuationMode::Overwrite;

        dag::BuildOptions options{
            .clusters_per_partition = args.clusters_per_partition,
            .target_ratio = args.target_ratio,
            .relative_target_error = args.target_error,
            .texture_options = {
                .atlas = {.padding = args.texture_gutter},
                .bake = {.reprojection = {.gutter = args.texture_gutter}},
                .sizing = args.sizing_options,
                .charting = args.charting,
                .allow_texture_reuse = args.allow_texture_reuse,
            },
            .root_node = args.root_node,
            .include_mode = args.include_mode,
            .write_debug_meshes = args.write_debug_meshes,
            .parallelize = args.parallelize,
            .continuation_mode = args.continuation_mode
        };
        // If the user specified neither target, fall back to a default error.
        if (!args.target_ratio && !args.target_error) {
            options.relative_target_error = 0.001f;
        }

        const auto build_result = dag::build_levels(
            input_storage,
            output_storage,
            options,
            args.level_range);
        if (!build_result) {
            LOG_ERROR("Invalid Structura Fundamentalis input: {}", build_result.error().to_string());
            return EXIT_FAILURE;
        }
        const auto index_result = output_storage.save_index();
        if (!index_result) {
            LOG_ERROR(
                "Failed to save output index in {}: {}",
                args.output_path,
                index_result.error().to_string());
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        LOG_ERROR("{}", e.what());
        return EXIT_FAILURE;
    }
}

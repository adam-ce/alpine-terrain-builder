#include <filesystem>
#include <numeric>
#include <string>
#include <vector>

#include "cli.h"
#include "cut.h"
#include "log.h"
#include "mask.h"
#include "merge.h"
#include "optional_utils.h"
#include "earth.h"
#include "store/describe_error.h"
#include "sf/Error.h"

std::optional<MeshMask> load_mask_from_path(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        LOG_INFO("Loading mask file from {}", path);
        const glm::dvec2 radius_range = mask::pad_radius_range(earth::radius_range(), 2);
        auto result = mask::load_from_path(path, radius_range);
        if (result.has_value()) {
            const auto mask = result.value();
            LOG_DEBUG("Loaded mask successfully ({} vertices, {} triangles)",
                mask.vertex_count(), mask.face_count());
            return mask;
        } else {
            LOG_ERROR("Failed to load mask: {}", result.error().description());
        }
    } else {
        LOG_DEBUG("No mask file found at {}, proceeding without mask", path);
    }

    return std::nullopt;
}

void run(const cli::MergeArgs& args) {
    LOG_TRACE("Loading base dataset from {}", args.base_path);
    auto base_result = octree::open_folder_indexed(args.base_path);
    if (!base_result.has_value()) {
        LOG_ERROR_AND_EXIT(
            "Failed to open base dataset {}: {}",
            args.base_path,
            store::describe_error(base_result.error()));
    }
    mesh::storage::IndexedStorage base_dataset = std::move(base_result.value());

    LOG_TRACE("Loading new dataset from {}", args.new_path);
    auto new_result = octree::open_folder_indexed(args.new_path);
    if (!new_result.has_value()) {
        LOG_ERROR_AND_EXIT(
            "Failed to open new dataset {}: {}",
            args.new_path,
            store::describe_error(new_result.error()));
    }
    mesh::storage::IndexedStorage new_dataset = std::move(new_result.value());

    LOG_TRACE("Creating output dataset at {}", args.output_path);
    std::filesystem::create_directories(args.output_path);
    octree::OpenOptions options;
    options.default_mapping = base_dataset.layout().mapping();
    options.preferred_extension = std::string(base_dataset.codec_selector().value_or(".sfmesh"));
    auto output_result = octree::open_folder(args.output_path, std::move(options));
    if (!output_result.has_value()) {
        LOG_ERROR_AND_EXIT(
            "Failed to open output dataset {}: {}",
            args.output_path,
            store::describe_error(output_result.error()));
    }
    mesh::storage::Storage output_dataset = std::move(output_result.value());

    std::optional<MeshMask> mask = flatten(map(args.mask_path, load_mask_from_path));

    const auto merge_result = merge_datasets(
        base_dataset,
        new_dataset,
        output_dataset,
        mask);
    if (!merge_result.has_value()) {
        LOG_ERROR_AND_EXIT("Failed to merge datasets: {}", sf::describe_error(merge_result.error()));
    }
}

void run(const cli::CutArgs& args) {
    LOG_TRACE("Loading input dataset from {}", args.input_path);
    auto input_result = octree::open_folder_indexed(args.input_path);
    if (!input_result.has_value()) {
        LOG_ERROR_AND_EXIT(
            "Failed to open input dataset {}: {}",
            args.input_path,
            store::describe_error(input_result.error()));
    }
    const mesh::storage::IndexedStorage input_dataset = std::move(input_result.value());
    const MeshMask mask = DEBUG_ASSERT_VAL(load_mask_from_path(args.mask_path)).value();
    const auto cut_result = cut_dataset(
        input_dataset,
        mask,
        args.output_path,
        args.keep_inside);
    if (!cut_result.has_value()) {
        LOG_ERROR_AND_EXIT("Failed to cut dataset: {}", sf::describe_error(cut_result.error()));
    }
}

void run(const cli::Args &args) {
    std::visit([](const auto& args) { run(args); }, args);
}

int main(int argc, char **argv) {
    const cli::Args args = cli::parse(argc, argv);
    const auto log_level = std::visit([](const auto& args) { return args.log_level; }, args);
    Log::init(log_level);

    const std::string arg_str = std::accumulate(argv, argv + argc, std::string(),
                                                [](const std::string &acc, const char *arg) {
                                                    return acc + (acc.empty() ? "" : " ") + arg;
                                                });
    LOG_DEBUG("Running with: {}", arg_str);

    run(args);
    // TODO: remove
    // std::filesystem::path source = "../../../meshes/out-merge";
    // std::filesystem::path destination = "/mnt/c/Users/Admin/Downloads/out-merge";
    // std::filesystem::remove_all(source);
    // std::filesystem::remove_all(destination);
    // std::filesystem::create_directories(destination);
    // std::filesystem::copy(source, destination,
    //             std::filesystem::copy_options::recursive |
    //             std::filesystem::copy_options::overwrite_existing);
}

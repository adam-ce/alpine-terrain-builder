#include <filesystem>
#include <numeric>
#include <string>
#include <vector>

#include "cli.h"
#include "cut.h"
#include "log.h"
#include "mask.h"
#include "merge.h"
#include "octree/Storage.h"
#include "optional_utils.h"
#include "earth.h"

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
    octree::IndexedStorage base_dataset = octree::open_folder_indexed(args.base_path);

    LOG_TRACE("Loading new dataset from {}", args.new_path);
    octree::IndexedStorage new_dataset = octree::open_folder_indexed(args.new_path);

    LOG_TRACE("Creating output dataset at {}", args.output_path);
    std::filesystem::create_directories(args.output_path);
    octree::Storage output_dataset = octree::open_folder(args.output_path, false, octree::OpenOptions{.preferred_extension_with_dot = std::string(base_dataset.layout().extension_with_dot())});

    std::optional<MeshMask> mask = flatten(map(args.mask_path, load_mask_from_path));

    return merge_datasets(base_dataset, new_dataset, output_dataset, mask);
}

void run(const cli::CutArgs& args) {
    LOG_TRACE("Loading input dataset from {}", args.input_path);
    const octree::IndexedStorage input_dataset = octree::open_folder_indexed(args.input_path);
    const MeshMask mask = DEBUG_ASSERT_VAL(load_mask_from_path(args.mask_path)).value();
    cut_dataset(input_dataset, mask, args.output_path, args.keep_inside);
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

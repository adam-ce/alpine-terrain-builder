#include <memory>
#include <optional>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <unordered_set>

#include <tl/expected.hpp>

#include "octree/storage/helpers.h"
#include "io/serialize.h"
#include "octree/NodeStatus.h"
#include "octree/IndexMap.h"
#include "octree/disk/IndexFile.h"
#include "octree/disk/Layout.h"
#include "octree/disk/layout/Strategy.h"
#include "octree/disk/layout/StrategyRegister.h"

namespace octree::helpers {
std::optional<std::unique_ptr<disk::layout::Strategy>> guess_layout_strategy(
    const std::filesystem::path &base_path,
    size_t max_files_to_check ) {

    if (!std::filesystem::is_directory(base_path)) {
        return std::nullopt;
    }

    std::vector<std::filesystem::path> candidate_paths;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(base_path)) {
        if (max_files_to_check == 0) {
            break;
        }
        if (!entry.is_regular_file() && !entry.is_symlink()) {
            continue;
        }

        const auto ext = entry.path().extension();
        // TODO: manage these somewhere else
        if (ext != ".terrain" && ext != ".glb" && ext != ".gltf") {
            continue;
        }

        candidate_paths.emplace_back(std::filesystem::relative(entry.path(), base_path));
        max_files_to_check--;
    }

    for (const auto &[_, make_strategy] : disk::layout::StrategyRegister::instance().factories()) {
        auto strategy = make_strategy();
        bool matches_all = std::all_of(candidate_paths.begin(), candidate_paths.end(),
                                       [&](const auto &rel_path) {
                                           return strategy->get_id_from_relative_node_path(rel_path).has_value();
                                       });

        if (matches_all) {
            return strategy;
        }
    }

    return std::nullopt;
}

tl::expected<void, io::Error> save_index_map(const IndexMap& index, const disk::Layout& layout) {
    const auto index_path = layout.base_path() / disk::v1::index_file_name();
    LOG_TRACE("Saving octree storage index to {}", index_path);

    disk::v1::IndexFile index_file;
    index_file.map = index;
    index_file.preferred_extension = layout.extension_with_dot();
    index_file.layout_strategy_id = disk::layout::StrategyRegister::instance().get_id(layout.strategy());

    const auto result = io::write_to_path(index_file, index_path);
    if (!result.has_value()) {
        LOG_ERROR("Failed to save octree storage index to {}", index_path);
        return tl::unexpected(result.error());
    }
    return {};
}

void update_index_map(IndexMap &index, const disk::Layout &layout) {
    index.clear();
    const auto &base_path = layout.base_path();

    std::filesystem::create_directories(base_path);

    for (const auto &entry : std::filesystem::recursive_directory_iterator(base_path)) {
        if (!entry.is_regular_file() && !entry.is_symlink()) {
            continue;
        }

        const auto ext = entry.path().extension();
        if (ext != ".terrain" && ext != ".glb" && ext != ".gltf") {
            continue;
        }

        auto id_opt = layout.get_id_from_node_path(entry.path());
        if (!id_opt) {
            continue;
        }

        const Id id = *id_opt;
        index.set_raw(id, NodeStatus::Leaf);
    }

    std::unordered_set<Id> visited;
    for (const auto &[id, _] : index) {
        auto parent = id.parent();
        while (parent) {
            if (visited.find(*parent) != visited.end()) {
                break;
            }
            visited.insert(*parent);

            if (index.get(*parent)) {
                index.set_raw(*parent, NodeStatus::Inner);
            } else {
                index.set_raw(*parent, NodeStatus::Virtual);
            }
            parent = parent->parent();
        }
    }
}

}

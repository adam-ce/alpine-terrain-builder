#include "octree/Storage.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <vector>
#include <unordered_set>

#include "log.h"
#include "mesh/io.h"
#include "octree/disk/IndexFile.h"
#include "octree/disk/layout/StrategyRegister.h"
#include "io/serialize.h"

namespace octree {

Storage::Storage(disk::Layout layout)
    : _layout(std::move(layout)) {}

Storage::Storage(IndexMap map, disk::Layout layout)
    : _index(std::move(map)), _layout(std::move(layout)) {}

std::optional<Node> Storage::read_node(const Id &id) const {
    const auto node_path = this->get_node_path(id);
    const auto result = mesh::io::load_from_path(node_path);
    if (result.has_value()) {
        return result.value();
    } else {
        return std::nullopt;
    }
}

bool Storage::write_node(const Id &id, const Node &node) const {
    const auto node_path = this->get_node_path(id);
    const auto result = mesh::io::save_to_path(node, node_path);
    return result.has_value();
}

bool Storage::has_node(const Id &id) const {
    if (this->_index.has_value()) {
        return this->_index->is_present(id);
    } else {
        return std::filesystem::exists(this->get_node_path(id));
    }
}

std::filesystem::path Storage::get_node_path(const Id &id) const {
    return this->_layout.get_node_path(id);
}

bool Storage::save_index() const {
    const auto index_path = this->_layout.base_path() / disk::v1::index_file_name();
    LOG_TRACE("Saving octree storage index to {}", index_path);

    disk::v1::IndexFile index_file;
    if (this->_index.has_value()) {
        index_file.map = this->_index.value();
    }
    index_file.preferred_extension = this->_layout.extension_with_dot();
    index_file.layout_strategy_id = disk::layout::StrategyRegister::instance().get_id(this->_layout.strategy());

    const auto result = io::write_to_path(index_file, index_path);
    if (!result.has_value()) {
        LOG_ERROR("Failed to save octree storage index to {}", index_path);
    }
    return result.has_value();
}

std::optional<Storage> load_index(const std::filesystem::path &index_path) {
    LOG_TRACE("Opening storage index {}", index_path);

    const auto result = io::read_from_path<disk::v1::IndexFile>(index_path);
    if (!result.has_value()) {
        LOG_TRACE("Failed to open storage index due to {}", result.error());
        return std::nullopt;
    }
    auto index_file = result.value();
    LOG_TRACE("Successfully read storage index with {} entries.", index_file.map.size());
    auto index_map = std::move(index_file.map);
    const auto base_path = index_path.parent_path();
    auto layout_strategy = disk::layout::StrategyRegister::instance().create(index_file.layout_strategy_id);
    disk::Layout layout(base_path, std::move(layout_strategy), index_file.preferred_extension);

    return Storage(std::move(index_map), std::move(layout));
}

namespace {
std::optional<std::unique_ptr<disk::layout::Strategy>> guess_layout_strategy(
    const std::filesystem::path &base_path,
    std::size_t max_files_to_check = 100) {

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

void update_index(IndexMap &index, const disk::Layout &layout) {
    index.clear();
    const auto &base_path = layout.base_path();

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
        index.set(id, NodeStatus::Leaf);
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
                index.set(*parent, NodeStatus::Inner);
            } else {
                index.set(*parent, NodeStatus::Virtual);
            }
            parent = parent->parent();
        }
    }
}
}

Storage open_folder(
    const std::filesystem::path &base_path,
    std::unique_ptr<disk::layout::Strategy> default_layout_strategy,
    const std::string extension_with_dot,
    bool save_index) {
    LOG_TRACE("Opening storage folder {}", base_path);

    if (!std::filesystem::is_directory(base_path)) {
        if (std::filesystem::exists(base_path)) {
            LOG_ERROR_AND_EXIT("Base path {} exists but is not a directory", base_path);
        }
        
        LOG_TRACE("Base path {} does not exist, creating it", base_path);
        std::filesystem::create_directories(base_path);
    }

    const std::filesystem::path index_path = base_path / disk::v1::index_file_name();
    auto storage_opt = load_index(index_path);
    if (storage_opt.has_value()) {
        LOG_TRACE("Loaded existing index");
        return std::move(storage_opt.value());
    }

    auto layout_strategy_opt = guess_layout_strategy(base_path);
    if (layout_strategy_opt) {
        LOG_TRACE("Guessed layout of dataset as {}", 
            disk::layout::StrategyRegister::instance().get_id(**layout_strategy_opt));
    } else {
        LOG_WARN("Unable to determine layout of dataset, using default which is {}", 
            disk::layout::StrategyRegister::instance().get_id(*default_layout_strategy));
        layout_strategy_opt = std::move(default_layout_strategy);
    }

    disk::Layout layout(base_path, std::move(*layout_strategy_opt), extension_with_dot);
    IndexMap map;
    update_index(map, layout);

    Storage storage(std::move(map), std::move(layout));
    if (save_index) {
        storage.save_index();
    }

    return storage;
}

} // namespace octree

#include <memory>
#include <optional>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <unordered_set>

#include <expected>

#include "octree/storage/helpers.h"
#include "io/serialize.h"
#include "octree/NodeStatus.h"
#include "octree/IndexMap.h"
#include "octree/disk/IndexFile.h"
#include "octree/disk/Layout.h"
#include "octree/disk/layout/Strategy.h"
#include "octree/disk/layout/StrategyRegister.h"

namespace octree::helpers {
namespace {
template <typename Key, typename Value>
std::optional<Key> find_max_key(const std::unordered_map<Key, Value>& map) {
    if (map.empty()) {
        return std::nullopt;
    }

    auto max_it = map.begin();
    for (auto it = std::next(max_it); it != map.end(); it++) {
        if (it->second > max_it->second) {
            max_it = it;
        }
    }

    return max_it->first;
}
}

std::optional<LayoutWithoutBase> guess_layout_strategy(
    const std::filesystem::path &base_path,
    size_t max_files_to_check) {

    if (!std::filesystem::is_directory(base_path)) {
        return std::nullopt;
    }

    std::vector<std::filesystem::path> candidate_paths;
    std::unordered_map<std::string, size_t> extension_counters;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(base_path)) {
        if (max_files_to_check == 0) {
            break;
        }
        if (!entry.is_regular_file() && !entry.is_symlink()) {
            continue;
        }

        const auto ext = entry.path().extension();
        if (ext == disk::v1::index_extension()) {
            continue;
        }
        extension_counters[ext]++;

        candidate_paths.emplace_back(std::filesystem::relative(entry.path(), base_path));
        max_files_to_check--;
    }
    std::optional<std::string> most_common_ext = find_max_key(extension_counters);

    std::unique_ptr<disk::layout::Strategy> best_strategy;
    size_t best_match_count = 0;
    const auto &factories = disk::layout::StrategyRegister::instance().factories();
    for (const auto &[_, make_strategy] : factories) {
        auto strategy = make_strategy(); // assume returns unique_ptr<Strategy>

        size_t match_count = 0;
        for (const auto &rel_path : candidate_paths) {
            if (strategy->get_id_from_relative_node_path(rel_path).has_value()) {
                match_count++;
            }
        }

        if (match_count > best_match_count) {
            best_match_count = match_count;
            best_strategy = std::move(strategy);
        }
    }

    if (best_strategy && most_common_ext.has_value()) {
        return LayoutWithoutBase(std::move(best_strategy), most_common_ext.value());
    }

    return std::nullopt;
}

std::expected<void, io::Error> save_index_map(const IndexMap& index, const disk::Layout& layout) {
    const auto index_path = layout.base_path() / disk::v1::index_file_name();
    LOG_TRACE("Saving octree storage index to {}", index_path);

    disk::v1::IndexFile index_file;
    index_file.map = index;
    index_file.preferred_extension = layout.extension_with_dot();
    index_file.layout_strategy_id = disk::layout::StrategyRegister::instance().get_id(layout.strategy());

    const auto result = io::write_to_path(index_file, index_path);
    if (!result.has_value()) {
        LOG_ERROR("Failed to save octree storage index to {}", index_path);
        return std::unexpected(result.error());
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
        if (ext == disk::v1::index_extension()) {
            continue;
        }

        auto id_opt = layout.get_id_from_node_path(entry.path());
        if (!id_opt) {
            continue;
        }

        const Id id = *id_opt;
        index.add(id);
    }
}

}

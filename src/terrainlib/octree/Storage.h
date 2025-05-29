#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "mesh/SimpleMesh.h"
#include "mesh/io.h"
#include "octree/Id.h"
#include "octree/IndexMap.h"
#include "octree/disk/Layout.h"
#include "octree/disk/layout/Strategy.h"
#include "octree/disk/layout/strategy/Default.h"

namespace octree {
using Node = SimpleMesh;

class Storage {
public:
    explicit Storage(disk::Layout layout);
    explicit Storage(IndexMap map, disk::Layout layout);

    std::optional<Node> read_node(const Id &id) const;
    bool write_node(const Id &id, const Node &node);
    bool remove_node(const Id &id);
    bool has_node(const Id &id) const;
    std::filesystem::path get_node_path(const Id &id) const;
    
    const IndexMap* index() const;
    bool save_index() const;

private:
    std::optional<IndexMap> _index;
    disk::Layout _layout;
};

std::optional<Storage> load_index(const std::filesystem::path &index_path);
Storage open_folder(
    const std::filesystem::path &base_path,
    std::unique_ptr<disk::layout::Strategy> default_layout_strategy = disk::layout::strategy::make_default(),
    const std::string extension_with_dot = ".terrain",
    bool save_index = true);

} // namespace octree

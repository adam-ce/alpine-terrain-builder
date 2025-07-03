#pragma once

#include <filesystem>

#include <tl/expected.hpp>

#include "mesh/io/error.h"
#include "octree/Id.h"
#include "octree/disk/Layout.h"
#include "octree/storage/Node.h"
#include "octree/storage/Error.h"

namespace octree {

class RawStorage {
public:
    explicit RawStorage(disk::Layout layout) noexcept;
    ~RawStorage() = default;
    RawStorage& operator=(const RawStorage&) = delete;
    RawStorage(const RawStorage&) = delete;
    RawStorage(RawStorage&&) = default;
    RawStorage& operator=(RawStorage&&) = default;

    tl::expected<Node, mesh::io::LoadMeshError> read_node(const Id &id) const noexcept;
    tl::expected<void, mesh::io::SaveMeshError> write_node(const Id &id, const Node &node) const noexcept;
    bool remove_node(const Id &id) const noexcept;
    bool has_node(const Id &id) const noexcept;
    std::filesystem::path get_node_path(const Id &id) const noexcept;
    std::filesystem::path base_path() const noexcept;

    const disk::Layout& layout() const noexcept;

private:
    disk::Layout _layout;
};

} // namespace octree

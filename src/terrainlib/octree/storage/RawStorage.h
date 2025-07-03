#pragma once

#include <filesystem>

#include <tl/expected.hpp>

#include "octree/Id.h"
#include "octree/storage/IStorage.h"
#include "octree/disk/Layout.h"
#include "mesh/io/error.h"

namespace octree {

class RawStorage : public IStorage {
public:
    explicit RawStorage(disk::Layout layout) noexcept;
    ~RawStorage() = default;
    RawStorage& operator=(const RawStorage&) = delete;
    RawStorage(const RawStorage&) = delete;
    RawStorage(RawStorage&&) = default;
    RawStorage& operator=(RawStorage&&) = default;

    tl::expected<Node, mesh::io::LoadMeshError> read_node(const Id &id) const noexcept override;
    tl::expected<void, mesh::io::SaveMeshError> write_node(const Id &id, const Node &node) noexcept override;
    bool remove_node(const Id &id) noexcept override;
    bool has_node(const Id &id) const noexcept override;
    std::filesystem::path get_node_path(const Id &id) const noexcept override;
    std::filesystem::path base_path() const noexcept override;

    const disk::Layout& layout() const noexcept;

private:
    disk::Layout _layout;
};

} // namespace octree

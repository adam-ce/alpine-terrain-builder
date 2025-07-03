#include "octree/Storage.h"

#include "mesh/io.h"

namespace octree {

RawStorage::RawStorage(disk::Layout layout) noexcept
    : _layout(std::move(layout)) {}

tl::expected<Node, mesh::io::LoadMeshError> RawStorage::read_node(const Id &id) const noexcept {
    const auto node_path = this->get_node_path(id);
    return mesh::io::load_from_path(node_path);
}

tl::expected<void, mesh::io::SaveMeshError> RawStorage::write_node(const Id &id, const Node &node) const noexcept {
    const auto node_path = this->get_node_path(id);
    return mesh::io::save_to_path(node, node_path);
}

tl::expected<void, CopyMeshError> RawStorage::copy_node_to(const Id &id, const RawStorage &target) const noexcept {
    const auto source_node_path = this->get_node_path(id);
    if (!this->has_node(id)) {
        return tl::unexpected(CopyMeshErrorKind::FileNotFound);
    }
    std::error_code ec;
    const auto target_node_path = target.get_node_path(id);
    if (std::filesystem::remove(target_node_path, ec)) {
        if (ec) {
            return tl::unexpected(CopyMeshErrorKind::RemoveOld);
        }
    }
    std::filesystem::create_directories(target_node_path.parent_path(), ec);
    if (ec) {
        return tl::unexpected(CopyMeshErrorKind::CreateDirectories);
    }
    std::filesystem::create_hard_link(
        source_node_path,
        target_node_path,
        ec);
    if (ec) {
        return tl::unexpected(CopyMeshErrorKind::CreateLink);
    }
    return {};
}

bool RawStorage::remove_node(const Id &id) const noexcept {
    const auto node_path = this->get_node_path(id);
    return std::filesystem::remove(node_path);
}

bool RawStorage::has_node(const Id &id) const noexcept {
    return std::filesystem::exists(this->get_node_path(id));
}

std::filesystem::path RawStorage::get_node_path(const Id &id) const noexcept {
    return this->_layout.get_node_path(id);
}

std::filesystem::path RawStorage::base_path() const noexcept {
    return this->_layout.base_path();
}

const disk::Layout& RawStorage::layout() const noexcept {
    return this->_layout;
}

} // namespace octree

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

tl::expected<void, CopyMeshError> RawStorage::copy_node_from(const Id &id, const RawStorage &source) noexcept {
    if (!source.has_node(id)) {
        return tl::unexpected(CopyMeshErrorKind::FileNotFound);
    }

    const auto source_node_path = source.get_node_path(id);
    const auto target_node_path = this->get_node_path(id);
    if (source_node_path.extension() != target_node_path.extension()) {
        // TODO: should this error instead?
        const auto load_result = mesh::io::load_from_path(source_node_path);
        if (!load_result.has_value()) {
            return tl::unexpected(CopyMeshErrorKind::Read);
        }
        const Node node = load_result.value();
        const auto save_result = mesh::io::save_to_path(node, target_node_path);
        if (!save_result.has_value()) {
            return tl::unexpected(CopyMeshErrorKind::Write);
        }
        return {};
    }

    std::error_code ec;
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

tl::expected<void, CopyMeshError> RawStorage::copy_node_to(const Id &id, RawStorage &target) const noexcept {
    return target.copy_node_from(id, *this);
}

bool RawStorage::remove_node(const Id &id) const noexcept {
    std::error_code ec;
    const auto removed = std::filesystem::remove(this->get_node_path(id), ec);
    return !ec && removed;
}

bool RawStorage::has_node(const Id &id) const noexcept {
    std::error_code ec;
    const auto exists = std::filesystem::exists(this->get_node_path(id), ec);
    return !ec && exists;
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

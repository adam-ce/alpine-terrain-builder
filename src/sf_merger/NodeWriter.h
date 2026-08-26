#pragma once

#include <expected>
#include <optional>

#include <libassert/assert.hpp>

#include "NodeLoader.h"
#include "mesh/SimpleMesh.h"
#include "octree/Id.h"
#include "mesh/storage.h"
#include "store/traverse.h"

// TODO: make thread safe
class NodeWriter {
public:
    NodeWriter(mesh::storage::Storage &storage) : _storage(storage) {}

    std::expected<bool, ::Error> has_node(
        const octree::Id &id) {
        return this->_storage.has(id);
    }

    std::expected<void, ::Error> write_node(
        const octree::Id &id,
        const SimpleMesh &mesh) {
        mesh::validate(mesh);
        const auto save_result = this->_storage.save(id, mesh);
        if (!save_result.has_value()) {
            return save_result;
        }
        const auto path_result = this->_storage.path_for(id);
        if (!path_result.has_value()) {
            return std::unexpected(path_result.error());
        }
        auto p = path_result.value();
        // change extension to .png
        p.replace_extension(".png");
        if (mesh.texture.has_value()) {
            cv::imwrite(p, mesh.texture.value());
        }
        return {};
    }

    std::expected<void, ::Error> copy_subtree_to_output(
        const octree::Id &id,
        const NodeLoader &loader) {
        std::optional<::Error> error;
        const auto traversal = store::traverse(
            loader.storage().index(),
            [&](const octree::Id &child_id, const store::NodeStatus &status) {
                if (error.has_value()) {
                    return;
                }
                if (status == store::NodeStatus::Virtual) {
                    return;
                }
                DEBUG_ASSERT(status == store::NodeStatus::Leaf);

                const auto result = this->_storage.copy_from(child_id, loader.storage());
                if (!result.has_value()) {
                    error = result.error();
                }
            },
            [&](const octree::Id &) { return !error.has_value(); },
            id);
        if (!traversal.has_value()) {
            return std::unexpected(traversal.error());
        }
        if (error.has_value()) {
            return std::unexpected(std::move(error.value()));
        }
        return {};
    }

private:
    mesh::storage::Storage &_storage;
};

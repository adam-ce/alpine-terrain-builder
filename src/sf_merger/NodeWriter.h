#pragma once

#include <expected>
#include <optional>

#include <libassert/assert.hpp>

#include "NodeLoader.h"
#include "mesh/SimpleMesh.h"
#include "octree/Id.h"
#include "octree/NodeStatus.h"
#include "octree/Storage.h"
#include "octree/traverse.h"

// TODO: make thread safe
class NodeWriter {
public:
    NodeWriter(octree::Storage &storage) : _storage(storage) {}

    std::expected<bool, store::FileOperationError<octree::Id>> has_node(
        const octree::Id &id) {
        return this->_storage.has(id);
    }

    std::expected<void, store::SaveError<octree::Id>> write_node(
        const octree::Id &id,
        const SimpleMesh &mesh) {
        mesh::validate(mesh);
        const auto save_result = this->_storage.save(id, mesh);
        if (!save_result.has_value()) {
            return save_result;
        }
        const auto path_result = this->_storage.path_for(id);
        if (!path_result.has_value()) {
            return std::unexpected(store::SaveError<octree::Id>(path_result.error()));
        }
        auto p = path_result.value();
        // change extension to .png
        p.replace_extension(".png");
        if (mesh.texture.has_value()) {
            cv::imwrite(p, mesh.texture.value());
        }
        return {};
    }

    std::expected<void, store::CopyError<octree::Id>> copy_subtree_to_output(
        const octree::Id &id,
        const NodeLoader &loader) {
        std::optional<store::CopyError<octree::Id>> error;
        octree::traverse(
            loader.storage().index(),
            [&](const octree::Id &child_id, const octree::NodeStatus &status) {
                if (error.has_value()) {
                    return;
                }
                if (status == octree::NodeStatus::Virtual) {
                    return;
                }
                DEBUG_ASSERT(status == octree::NodeStatus::Leaf);

                const auto result = this->_storage.copy_from(child_id, loader.storage());
                if (!result.has_value()) {
                    error = result.error();
                }
            },
            [&](const octree::Id &) { return !error.has_value(); },
            id);
        if (error.has_value()) {
            return std::unexpected(std::move(error.value()));
        }
        return {};
    }

private:
    octree::Storage &_storage;
};

#pragma once

#include <expected>
#include <optional>

#include <libassert/assert.hpp>
#include <opencv2/imgcodecs.hpp>

#include "Error.h"
#include "NodeLoader.h"
#include "mesh/SimpleMesh.h"
#include "octree/Id.h"
#include "mesh/storage.h"
#include "store/traverse.h"

// TODO: make thread safe
class NodeWriter {
public:
    NodeWriter(mesh::storage::Storage &storage) : _storage(storage) {}

    Expected<bool> has_node(
        const octree::Id &id) {
        return this->_storage.has(id);
    }

    Expected<void> write_node(
        const octree::Id &id,
        const SimpleMesh &mesh) {
        mesh::validate(mesh);
        auto save_result = this->_storage.save(id, mesh);
        if (!save_result) {
            return save_result;
        }
        auto path_result = this->_storage.path_for(id);
        if (!path_result) {
            return Error::propagate(
                std::move(path_result), "resolve texture output path for node " + id.to_string());
        }
        auto p = path_result.value();
        // change extension to .png
        p.replace_extension(".png");
        if (mesh.texture.has_value()) {
            try {
                if (!cv::imwrite(p, mesh.texture.value())) {
                    return Error::fail(Error::Code::Io, "write node texture to", p);
                }
            } catch (const cv::Exception& error) {
                return Error::fail(
                    Error::Code::Io, "write node texture to \"" + p.string() + "\": " + error.what());
            }
        }
        return {};
    }

    Expected<void> copy_subtree_to_output(
        const octree::Id &id,
        const NodeLoader &loader) {
        std::optional<Error> error = std::nullopt;
        auto traversal = store::traverse(
            loader.storage().index(),
            [&](const octree::Id &child_id, const store::NodeStatus &status) {
                if (error.has_value()) {
                    return;
                }
                if (status == store::NodeStatus::Virtual) {
                    return;
                }
                DEBUG_ASSERT(status == store::NodeStatus::Leaf);

                auto result = this->_storage.copy_from(child_id, loader.storage());
                if (!result) {
                    error = std::move(result).error();
                }
            },
            [&](const octree::Id &) { return !error.has_value(); },
            id);
        if (!traversal) {
            return Error::propagate(
                std::move(traversal), "traverse source subtree rooted at " + id.to_string() + " while copying");
        }
        if (error.has_value()) {
            return Error::propagate(
                std::move(error.value()), "copy subtree rooted at " + id.to_string() + " to output");
        }
        return {};
    }

private:
    mesh::storage::Storage &_storage;
};

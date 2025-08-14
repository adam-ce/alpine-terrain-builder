#pragma once

#include <libassert/assert.hpp>

#include "NodeLoader.h"
#include "mesh/SimpleMesh.h"
#include "octree/Id.h"
#include "octree/NodeStatus.h"
#include "octree/storage/Storage.h"
#include "octree/traverse.h"

// TODO: make thread safe
class NodeWriter {
public:
    NodeWriter(octree::Storage &storage) : _storage(storage) {}

    bool has_node(const octree::Id &id) {
        if (this->_overwrite) {
            return false;
        } else {
            return this->_storage.has_node(id);
        }
    }

    void write_node(const octree::Id &id, const SimpleMesh &mesh) {
        DEBUG_ASSERT_VAL(this->_storage.write_node(id, mesh, this->_overwrite));
    }

    void copy_subtree_to_output(
        const octree::Id &id,
        const NodeLoader &loader) {
        octree::traverse(
            loader.storage().index(),
            [&](const octree::Id &child_id, const octree::NodeStatus &status) {
                if (status == octree::NodeStatus::Virtual) {
                    return;
                }
                DEBUG_ASSERT(status == octree::NodeStatus::Leaf);

                DEBUG_ASSERT_VAL(this->_storage.copy_node_from(child_id, loader.storage()));
            },
            octree::always_refine,
            id);
    }

private:
    octree::Storage &_storage;
    bool _overwrite = false;
};

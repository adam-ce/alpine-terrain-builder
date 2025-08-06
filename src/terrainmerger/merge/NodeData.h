#pragma once

#include <optional>

#include "NodeLoader.h"
#include "log.h"
#include "mesh/SimpleMesh.h"
#include "octree/Id.h"
#include "octree/NodeStatusOrMissing.h"
#include "octree/Storage.h"

namespace merge {

template <octree::NodeStatusOrMissing Status>
class NodeData {
public:
    constexpr explicit NodeData(octree::Id id, const NodeLoader &loader) : _id(id), _loader(loader) {}

    const SimpleMesh &mesh() const {
        static_assert(Status != octree::NodeStatusOrMissing::Missing, "Trying to access mesh from missing node.");

        if (!this->_mesh.has_value()) {
            auto result = this->_loader.load_node(this->_id);
            if (result.has_value()) {
                this->_mesh = result.value();
            } else {
                LOG_ERROR_AND_EXIT("Failed to read node from loader that should be present");
            }
        }
        return this->_mesh.value();
    }

    static constexpr octree::NodeStatusOrMissing status() {
        return Status;
    }

private:
    octree::Id _id;
    const NodeLoader &_loader;
    mutable std::optional<SimpleMesh> _mesh;
};

} // namespace merge

#pragma once

#include <optional>

#include "NodeLoader.h"
#include "log.h"
#include "mesh/SimpleMesh.h"
#include "octree/Id.h"
#include "octree/NodeStatusOrMissing.h"
#include "octree/Storage.h"
#include "optional_utils.h"

namespace merge {

template <octree::NodeStatusOrMissing Status>
class NodeData {
public:
    constexpr explicit NodeData(octree::Id id, const NodeLoader &loader) : _id(id), _loader(loader) {}

    static constexpr octree::NodeStatusOrMissing status() {
        return Status;
    }

    octree::Id id() const {
        return this->_id;
    }

    // Status::Leaf or Status::Inner guarantuess a node is present
    template <octree::NodeStatusOrMissing S = Status>
    std::enable_if_t<S == octree::NodeStatusOrMissing::Leaf || S == octree::NodeStatusOrMissing::Inner, const SimpleMesh &>
    mesh() const {
        auto mesh = this->load_mesh();
        if (mesh.has_value()) {
            return this->_mesh.value();
        } else {
            LOG_ERROR_AND_EXIT("Failed to read node from loader that should be present");
        }
    }

    // Status::Missing and Status::Virtual do not exist on disk, but may be the child of a leaf or inner node
    template <octree::NodeStatusOrMissing S = Status>
    std::enable_if_t<S == octree::NodeStatusOrMissing::Missing || S == octree::NodeStatusOrMissing::Virtual, std::optional<std::reference_wrapper<SimpleMesh>>>
    mesh() const {
        return this->load_mesh();
    }

private:
    std::optional<std::reference_wrapper<SimpleMesh>> load_mesh() const {
        if (!_mesh.has_value()) {
            auto result = this->_loader.load_node(this->_id);
            if (result.has_value()) {
                this->_mesh = result.value();
            }
        }
        return as_ref(this->_mesh);
    }

    octree::Id _id;
    const NodeLoader &_loader;
    mutable std::optional<SimpleMesh> _mesh;
};

} // namespace merge

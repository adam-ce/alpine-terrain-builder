/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 * Copyright (C) 2025 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#pragma once

#include <optional>

#include "NodeLoader.h"
#include "log.h"
#include "mesh/SimpleMesh.h"
#include "octree/Id.h"
#include "optional_utils.h"
#include "store/NodeStatusOrMissing.h"

namespace merge {

template <store::NodeStatusOrMissing Status>
class NodeData {
public:
    constexpr explicit NodeData(octree::Id id, const NodeLoader &loader) : _id(id), _loader(loader) {}

    static constexpr store::NodeStatusOrMissing status() {
        return Status;
    }

    octree::Id id() const {
        return this->_id;
    }

    // Status::Leaf or Status::Inner guarantuess a node is present
    template <store::NodeStatusOrMissing S = Status>
    std::enable_if_t<S == store::NodeStatusOrMissing::Leaf || S == store::NodeStatusOrMissing::Inner, const SimpleMesh &>
    mesh() const {
        auto mesh = this->load_mesh();
        if (mesh.has_value()) {
            return this->_mesh.value();
        } else {
            LOG_ERROR_AND_EXIT("Failed to read node from loader that should be present");
        }
    }

    // Status::Missing and Status::Virtual do not exist on disk, but may be the child of a leaf or inner node
    template <store::NodeStatusOrMissing S = Status>
    std::enable_if_t<S == store::NodeStatusOrMissing::Missing || S == store::NodeStatusOrMissing::Virtual, std::optional<std::reference_wrapper<SimpleMesh>>>
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

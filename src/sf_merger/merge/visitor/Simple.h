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

#include "merge/Result.h"
#include "merge/NodeData.h"
#include "merge/visitor/Visitor.h"
#include "mesh/merge.h"
#include "octree/Id.h"
#include "store/NodeStatusOrMissing.h"

namespace merge::visitor {

class Simple {
public:
    using Status = store::NodeStatusOrMissing;
    struct Context{};
    using Result = merge::Result<Context>;

    Context make_root_context() {
        return {};
    }

    template <Status LeftStatus, Status RightStatus>
    Result visit(
        const octree::Id &,
        const NodeData<LeftStatus> &left,
        const NodeData<RightStatus> &right,
        const Context& ctx) {

        if constexpr (RightStatus == Status::Missing) {
            return Unchanged{Source::Left};
        }

        if constexpr (LeftStatus == Status::Missing) {
            return Unchanged{Source::Right};
        }

        if constexpr (LeftStatus == Status::Leaf && RightStatus == Status::Leaf) {
            return merge_meshes(right.mesh(), left.mesh());
        }

        return Recurse{ctx};
    }

private:
    Result merge_meshes(
        const SimpleMesh &base_mesh, // left
        const SimpleMesh &new_mesh // right
    ) {
        // If one mesh is empty, return the other
        if (base_mesh.is_empty()) {
            return Unchanged { Source::Right };
        }
        if (new_mesh.is_empty()) {
            return Unchanged { Source::Left };
        }

        // TODO: this doesnt really work well if there are intersections
        const SimpleMesh merged_mesh = mesh::merge(base_mesh, new_mesh);
        return Merged{merged_mesh};
    }
};

static_assert(Visitor<Simple>);

}

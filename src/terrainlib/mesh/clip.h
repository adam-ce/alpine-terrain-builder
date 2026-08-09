#pragma once

#include <radix/geometry.h>

#include "mesh/SimpleMesh.h"
#include "containers/Cow.h"

namespace mesh {

Cow<const SimpleMesh> clip_on_bounds(const SimpleMesh &mesh, const radix::geometry::Aabb3d &bounds);
Cow<const SimpleMesh> clip_on_bounds_and_cap(
    const SimpleMesh &mesh,
    const radix::geometry::Aabb3d &bounds,
    const bool remesh_planar_patches = true);
Cow<const SimpleMesh> clip_on_mesh(const SimpleMesh &mesh, const SimpleMesh& clip_mesh, const bool keep_inside = true);

}

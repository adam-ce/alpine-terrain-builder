#pragma once

#include <radix/geometry.h>

#include "mesh/SimpleMesh.h"

namespace mesh {
SimpleMesh clip_on_bounds(const SimpleMesh &mesh, const radix::geometry::Aabb3d &bounds);
SimpleMesh clip_on_mesh(const SimpleMesh &mesh, const SimpleMesh& clip_mesh);
}

#pragma once

#include <span>

#include "polygon/Polygon.h"
#include "mesh/SimpleMesh.h"

namespace polygon {

SimpleMesh3d triangulate(const Polygon3d &polygon);
void triangulate(SimpleMesh3d &mesh, const std::span<const uint32_t> indices);

} // namespace polygon

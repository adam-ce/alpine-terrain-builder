#pragma once

#include <span>

#include "mesh/SimpleMesh.h"
#include "mesh/merging/VertexDeduplicate.h"
#include "mesh/merging/VertexMapping.h"

namespace mesh::merging {

// VertexMapping create_connecting_mapping(std::span<const SimpleMesh> meshes);
VertexMapping create_mapping(
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes,
    double distance_epsilon);
VertexMapping create_mapping(
    std::span<const std::reference_wrapper<const SimpleMesh>> meshes, 
    VertexDeduplicate<3, double, VertexId>& deduplicate);

SimpleMesh apply_mapping(
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes,
    const VertexMapping &mapping);

} // namespace mesh::merging

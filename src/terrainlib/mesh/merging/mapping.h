#pragma once

#include <span>
#include <functional>

#include "mesh/SimpleMesh.h"
#include "mesh/merging/VertexDeduplicate.h"
#include "mesh/merging/VertexMapping.h"

namespace mesh::merging {

// VertexMapping create_connecting_mapping(std::span<const SimpleMesh> meshes);

// TODO: add option structs
VertexMapping create_mapping(
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes);
VertexMapping create_mapping(
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes,
    const double distance_epsilon,
    const bool only_consider_boundary = false);
VertexMapping create_mapping(
    std::span<const std::reference_wrapper<const SimpleMesh>> meshes, 
    VertexDeduplicate<3, double, VertexId>& deduplicate,
    const bool only_consider_boundary = false);

SimpleMesh apply_mapping(
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes,
    const VertexMapping &mapping,
    const bool deduplicate_triangles = true,
    const bool merge_uvs = true
);

} // namespace mesh::merging

#pragma once

#include <span>
#include <vector>

#include "mesh/SimpleMesh.h"
#include "mesh/merging/VertexMapping.h"

namespace mesh {
std::vector<std::vector<uint32_t>> find_boundaries(const SimpleMesh &mesh);
std::vector<std::vector<uint32_t>> find_holes(const SimpleMesh &mesh);
std::vector<std::vector<uint32_t>> find_holes_on_merge_border(const SimpleMesh &mesh, const mesh::merging::VertexMapping &mapping);
void fill_planar_hole(SimpleMesh &mesh, const std::span<const uint32_t> hole);
void fill_planar_holes(SimpleMesh &mesh, const std::vector<std::vector<uint32_t>> holes);
void fill_holes_on_merge_border(SimpleMesh &mesh, const mesh::merging::VertexMapping &mapping);
std::vector<std::vector<merging::VertexId>> find_holes_between_meshes(
    const std::span<const std::reference_wrapper<const SimpleMesh>> &meshes,
    const merging::VertexMapping &mapping
);
} // namespace mesh
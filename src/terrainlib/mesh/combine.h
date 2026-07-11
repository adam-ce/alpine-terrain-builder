#pragma once

#include <span>
#include <functional>

#include "mesh/SimpleMesh.h"
#include "mesh/merging/VertexMapping.h"

namespace mesh {

template <typename MeshRange>
auto combine(
    const MeshRange &meshes,
    std::vector<size_t> &vertex_offsets) {
    using Mesh = std::remove_cvref_t<std::unwrap_reference_t<std::ranges::range_value_t<MeshRange>>>;
    using Uv = typename Mesh::Uv;

    size_t combined_vertex_count = 0;
    size_t combined_triangle_count = 0;
    for (const Mesh &mesh : meshes) {
        combined_vertex_count += mesh.vertex_count();
        combined_triangle_count += mesh.face_count();
    }

    bool has_uvs = false;
    for (const Mesh &mesh : meshes) {
        if (mesh.has_uvs()) {
            has_uvs = true;
            break;
        }
    }

    Mesh result;
    result.positions.reserve(combined_vertex_count);
    result.triangles.reserve(combined_triangle_count);
    if (has_uvs) {
        result.uvs.reserve(combined_vertex_count);
    }

    size_t vertex_offset = 0;
    for (size_t mesh_index = 0; mesh_index < meshes.size(); mesh_index++) {
        const Mesh &mesh = meshes[mesh_index];
        result.positions.insert(result.positions.end(), mesh.positions.begin(), mesh.positions.end());

        if (has_uvs) {
            if (mesh.has_uvs()) {
                result.uvs.insert(result.uvs.end(), mesh.uvs.begin(), mesh.uvs.end());
            } else {
                result.uvs.insert(result.uvs.end(), mesh.vertex_count(), Uv(0));
            }
        }

        for (const auto &triangle : mesh.triangles) {
            result.triangles.push_back(triangle + static_cast<uint32_t>(vertex_offset));
        }

        vertex_offsets.push_back(vertex_offset);
        vertex_offset += mesh.vertex_count();
    }

    return result;
}

template <typename MeshRange>
auto combine(const MeshRange &meshes) {
    std::vector<size_t> vertex_offsets;
    return combine(meshes, vertex_offsets);
}

template <glm::length_t n_dims, typename T>
void combine_inplace(
    SimpleMesh_<n_dims, T> &acc_mesh,
    const SimpleMesh_<n_dims, T> &other_mesh) {
    bool has_uvs = acc_mesh.has_uvs() || other_mesh.has_uvs();

    const auto vertex_offset = acc_mesh.vertex_count();
    acc_mesh.positions.insert(acc_mesh.positions.end(), other_mesh.positions.begin(), other_mesh.positions.end());

    if (has_uvs) {
        if (other_mesh.has_uvs()) {
            acc_mesh.uvs.insert(acc_mesh.uvs.end(), other_mesh.uvs.begin(), other_mesh.uvs.end());
        } else {
            acc_mesh.uvs.insert(acc_mesh.uvs.end(), other_mesh.vertex_count(), glm::dvec2(0));
        }
    }

    acc_mesh.triangles.reserve(acc_mesh.face_count() + other_mesh.face_count());
    for (const auto &triangle : other_mesh.triangles) {
        acc_mesh.triangles.push_back(triangle + static_cast<uint32_t>(vertex_offset));
    }
}
}

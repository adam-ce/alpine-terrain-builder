/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
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

#include <libassert/assert.hpp>

#include "cluster.h"
#include "utils.h"
#include "mesh/validate.h"

inline void validate(const Cluster &cluster, const std::span<const glm::dvec3> positions) {
    const size_t cluster_vertex_count = cluster.vertex_indices.size();
    const size_t mesh_vertex_count = positions.size();

    // Cluster must have vertices
    ASSERT(!cluster.vertex_indices.empty(), "Cluster has zero vertices");

    // Cluster must have triangles
    ASSERT(!cluster.local_triangles.empty(), "Cluster has zero triangles");

    // Validate vertex indices reference valid mesh vertices
    for (const uint32_t vertex_index : cluster.vertex_indices) {
        ASSERT(vertex_index < mesh_vertex_count, "cluster.vertex_indices contains out-of-range vertex index");
    }

    // Validate triangle indices reference cluster-local vertices
    for (const glm::uvec3 &triangle : cluster.local_triangles) {
        for (uint8_t corner = 0; corner < 3; corner++) {
            ASSERT(triangle[corner] < cluster_vertex_count,
                    "cluster.local_triangles refers to invalid local vertex index");
        }
    }

    // Materialize cluster mesh to validate mesh
    const mesh::Simple mesh = materialize_cluster(cluster, positions);
    mesh::validate_basic(mesh);
}

inline void validate(const Clustering &clustering) {
    // ASSERT(!clustering.positions.empty(), "Clustering must have positions.");

    const uint32_t cluster_count = static_cast<uint32_t>(clustering.clusters.size());

    for (uint32_t cluster_index = 0; cluster_index < cluster_count; cluster_index++) {
        const Cluster &cluster = clustering.clusters[cluster_index];
        validate(cluster, clustering.positions);
        ASSERT(!cluster.is_textured() || cluster.texture_id.value() < clustering.textures.size());
        ASSERT(cluster.is_textured() == cluster.has_uvs());
    }
}

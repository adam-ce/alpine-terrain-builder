#include <algorithm>
#include <cstddef>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>
#include <functional>

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <libassert/assert.hpp>
#include <radix/geometry.h>

#include "build_config.h"
#include "containers/UnionFind.h"
#include "log.h"
#include "mesh/convert.h"
#include "mesh/connectivity/boundary.h"
#include "mesh/merging/EpsilonVertexDeduplicate.h"
#include "mesh/merging/helpers.h"
#include "mesh/merging/mapping.h"
#include "mesh/validate.h"
#include "spatial_lookup/Grid.h"
#include "spatial_lookup/Hashmap.h"
#include "type_utils.h"
#include "range_utils.h"

namespace mesh::merging::detail {
void validate_epsilon_mapping(
    const VertexMapping &mapping,
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes,
    double epsilon) {
    if constexpr (IS_DEBUG_BUILD) {
        const double epsilon2 = epsilon * epsilon;

        const uint32_t max_merged_index = mapping.find_max_merged_index();

        std::vector<VertexId> originals;
        for (uint32_t merged_index = 0; merged_index <= max_merged_index; merged_index++) {
            // Collect all original vertices mapping to this merged index
            originals.clear();
            for (uint32_t mesh_index = 0; mesh_index < mapping.mesh_count(); mesh_index++) {
                const std::optional<uint32_t> vertex_index = mapping.map_backward(mesh_index, merged_index);
                if (vertex_index.has_value()) {
                    originals.push_back(VertexId{.mesh_index = mesh_index, .vertex_index = vertex_index.value()});
                }
            }

            // Check all pairs of original vertices for this merged vertex
            for (uint32_t i = 0; i < originals.size(); i++) {
                const glm::dvec3 &pi = meshes[originals[i].mesh_index].get().positions[originals[i].vertex_index];
                for (uint32_t j = i + 1; j < originals.size(); j++) {
                    const glm::dvec3 &pj = meshes[originals[j].mesh_index].get().positions[originals[j].vertex_index];
                    const double dist2 = glm::distance2(pi, pj);
                    DEBUG_ASSERT(dist2 <= epsilon2);
                }
            }
        }
    }
}

VertexMapping create_mapping(
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes,
    const ResolvedCreateOptions options
) {
    VertexDeduplicate<3, double, VertexId> &deduplicate = options.deduplicate;
    const bool only_consider_boundary = options.only_consider_boundary;

    if (meshes.empty()) {
        return {};
    }
    if (meshes.size() == 1) {
        return VertexMapping::identity(meshes[0].get().vertex_count());
    }

    LOG_TRACE("Finding shared vertices between {} meshes using {}", meshes.size(), type_name(deduplicate));
    for (uint32_t i = 0; i < meshes.size(); i++) {
        const SimpleMesh &mesh = meshes[i].get();
        LOG_TRACE("Mesh {}: {} vertices, {} triangles", i, mesh.vertex_count(), mesh.face_count());
    }

    std::vector<uint32_t> mesh_sizes;
    mesh_sizes.reserve(meshes.size());
    std::transform(meshes.begin(), meshes.end(),
                   std::back_inserter(mesh_sizes),
                   [](const auto &mesh) { return mesh.get().vertex_count(); });
    const uint32_t maximal_merged_mesh_size = std::accumulate(mesh_sizes.begin(), mesh_sizes.end(), 0u);
    // Handle all meshes being empty
    if (maximal_merged_mesh_size == 0) {
        return {};
    }

    VertexMapping mapping;
    mapping.init(mesh_sizes);

    uint32_t unique_vertices = 0;
    auto add_unique_vertex = [&](const VertexId &vertex) {
        mapping.add(vertex, unique_vertices);
        unique_vertices += 1;
    };

    // Init a reusable set of containers for the boundary
    std::unordered_set<glm::uvec2> boundary_edges;
    std::vector<bool> is_boundary_vertex;

    // Reusable vector for duplicate vertices
    std::vector<VertexId> duplicate_vertices;

    bool has_warned = false; // Flag to only print the intra-mesh merge warning once
    for (uint32_t mesh_index = 0; mesh_index < meshes.size(); mesh_index++) {
        const SimpleMesh &mesh = meshes[mesh_index];

        if (only_consider_boundary) {
            // Find boundary edges
            boundary_edges.clear();
            find_boundary_edges(mesh.triangles, boundary_edges);

            // Classify vertices as on the boundary or on the inside
            is_boundary_vertex.assign(mesh.vertex_count(), false);
            for (const auto& edge : boundary_edges) {
                is_boundary_vertex[edge[0]] = true;
                is_boundary_vertex[edge[1]] = true;
            }
        }

        for (uint32_t vertex_index = 0; vertex_index < mesh.vertex_count(); vertex_index++) {
            const glm::dvec3 &position = mesh.positions[vertex_index];
            const VertexId current_vertex{
                .mesh_index = mesh_index,
                .vertex_index = vertex_index};

            if (only_consider_boundary && !is_boundary_vertex[vertex_index]) {
                add_unique_vertex(current_vertex);
            } else if (mesh_index == 0) {
                add_unique_vertex(current_vertex);
                deduplicate.insert(position, current_vertex);
            } else if (!deduplicate.find_or_insert(position, current_vertex, duplicate_vertices)) {
                // Duplicates detected
                std::optional<std::pair<VertexId, double>> nearest_duplicate;
                for (const VertexId &other_vertex : duplicate_vertices) {
                    // Warn if we would perform intra mesh merges (but dont actually do them)
                    if (other_vertex.mesh_index == current_vertex.mesh_index) {
                        if (!has_warned) {
                            LOG_WARN("Deduplication is too inclusive and would perform intra-mesh merges");
                            has_warned = true;
                        }
                    } else {
                        double distance2 = 0;
                        // Only calculate the distance if there is actually more than a single duplicate vertex
                        if (duplicate_vertices.size() > 1) {
                            distance2 = glm::distance2(meshes[other_vertex.mesh_index].get().positions[other_vertex.vertex_index], position);
                        }
                        if (!nearest_duplicate.has_value() || nearest_duplicate.value().second > distance2) {
                            nearest_duplicate = {other_vertex, distance2};
                        }
                    }
                }
                if (nearest_duplicate.has_value()) {
                    const auto mapped = mapping.map_forward(nearest_duplicate.value().first);
                    DEBUG_ASSERT(!mapping.map_backward(mesh_index, mapped).has_value());
                    mapping.add(current_vertex, mapped);
                } else {
                    deduplicate.insert(position, current_vertex);
                    add_unique_vertex(current_vertex);
                }
                duplicate_vertices.clear();
            } else {
                // New vertex / No duplicates
                add_unique_vertex(current_vertex);
            }
        }
    }

    LOG_DEBUG("Identified {} shared and {} unique vertices", maximal_merged_mesh_size - unique_vertices, unique_vertices);
    mapping.validate();

    if constexpr (IS_DEBUG_BUILD) {
        using DefaultEpsilonDeduplicate = EpsilonVertexDeduplicate<3, double, VertexId, spatial_lookup::Grid3d<VertexId>>;
        const auto *epsilon_deduplicate = dynamic_cast<DefaultEpsilonDeduplicate*>(&deduplicate);
        if (epsilon_deduplicate != nullptr) {
            const double epsilon = epsilon_deduplicate->epsilon();
            validate_epsilon_mapping(mapping, meshes, epsilon);
        }
    }

    return mapping;
}

SimpleMesh apply_mapping(
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes,
    const VertexMapping &mapping,
    const ResolvedApplyOptions options
) {
    const bool deduplicate_triangles = options.deduplicate_triangles;
    const bool merge_uvs = options.merge_uvs;

    LOG_TRACE("Merging meshes based on mapping");
    if (meshes.empty()) {
        return {};
    }

    SimpleMesh merged_mesh;

    uint32_t max_combined_vertex_count = 0;
    uint32_t max_combined_face_count = 0;
    for (const SimpleMesh &mesh : meshes) {
        max_combined_vertex_count += mesh.vertex_count();
        max_combined_face_count += mesh.face_count();
    }

    // Can happen if all meshes are empty
    if (max_combined_vertex_count == 0) {
        return {};
    }

    const bool has_uvs = merge_uvs && std::any_of(meshes.begin(), meshes.end(), [](const SimpleMesh &mesh) {
        return mesh.has_uvs();
    });

    merged_mesh.positions.resize(max_combined_vertex_count);
    if (has_uvs) {
        merged_mesh.uvs.resize(max_combined_vertex_count);
    }

    uint32_t max_vertex_index = 0;
    for (uint32_t mesh_index = 0; mesh_index < meshes.size(); mesh_index++) {
        const SimpleMesh &mesh = meshes[mesh_index];
        for (uint32_t vertex_index = 0; vertex_index < mesh.vertex_count(); vertex_index++) {
            const uint32_t mapped_index = mapping.map_forward(VertexId{.mesh_index = mesh_index, .vertex_index = vertex_index});
            merged_mesh.positions[mapped_index] = mesh.positions[vertex_index];
            if (has_uvs) {
                merged_mesh.uvs[mapped_index] = mesh.has_uvs() ? mesh.uvs[vertex_index] : glm::dvec2(0);
            }
            max_vertex_index = std::max(max_vertex_index, mapped_index);
        }
    }
    if (max_vertex_index == 0) {
        return {};
    }
    
    DEBUG_ASSERT(max_vertex_index < max_combined_vertex_count);
    merged_mesh.positions.resize(max_vertex_index + 1);
    if (has_uvs) {
        merged_mesh.uvs.resize(max_vertex_index + 1);
    }

    merged_mesh.triangles.reserve(max_combined_face_count);
    for (uint32_t mesh_index = 0; mesh_index < meshes.size(); mesh_index++) {
        const SimpleMesh &mesh = meshes[mesh_index];
        for (uint32_t triangle_index = 0; triangle_index < mesh.face_count(); triangle_index++) {
            const glm::uvec3 &triangle = mesh.triangles[triangle_index];

            glm::uvec3 new_triangle;
            for (uint32_t k = 0; k < static_cast<uint32_t>(triangle.length()); k++) {
                new_triangle[k] = mapping.map_forward(VertexId{.mesh_index = mesh_index, .vertex_index = triangle[k]});
            }
            if (new_triangle[0] == new_triangle[1] ||
                new_triangle[1] == new_triangle[2] ||
                new_triangle[2] == new_triangle[0]) {
                LOG_WARN("Skipping degenerate triangle while merging");
                continue;
            }

            // Deduplicate triangles
            if (deduplicate_triangles) {
                const bool is_first_mesh = std::ranges::none_of(
                    range(mesh_index),
                    [&](const uint32_t i) { return mapping.find_source_triangle_in_mesh(new_triangle, i).has_value(); });

                if (!is_first_mesh) {
                    LOG_WARN("Skipping duplicate triangle while merging");
                    continue;
                }
            }

            merged_mesh.triangles.push_back(new_triangle);
        }
    }

    mesh::validate_basic(merged_mesh);

    return merged_mesh;
}
} // namespace mesh::merging::detail

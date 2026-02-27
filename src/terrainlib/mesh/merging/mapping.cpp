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

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <libassert/assert.hpp>
#include <radix/geometry.h>

#include "UnionFind.h"
#include "log.h"
#include "mesh/convert.h"
#include "mesh/topology.h"
#include "mesh/merging/EpsilonVertexDeduplicate.h"
#include "mesh/merging/helpers.h"
#include "mesh/merging/mapping.h"
#include "mesh/validate.h"
#include "spatial_lookup/Grid.h"
#include "spatial_lookup/Hashmap.h"
#include "type_utils.h"

namespace mesh::merging::detail {
void validate_epsilon_mapping(
    const VertexMapping &mapping,
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes,
    double epsilon) {
    USE(mapping, meshes, epsilon);
#ifndef NDEBUG
    const double epsilon2 = epsilon * epsilon;

    const size_t max_merged_index = mapping.find_max_merged_index();

    std::vector<VertexId> originals;
    for (size_t merged_index = 0; merged_index <= max_merged_index; merged_index++) {
        // Collect all original vertices mapping to this merged index
        originals.clear();
        for (size_t mesh_index = 0; mesh_index < mapping.mesh_count(); mesh_index++) {
            const std::optional<size_t> vertex_index = mapping.map_backward(mesh_index, merged_index);
            if (vertex_index.has_value()) {
                originals.push_back(VertexId{.mesh_index = mesh_index, .vertex_index = vertex_index.value()});
            }
        }

        // Check all pairs of original vertices for this merged vertex
        for (size_t i = 0; i < originals.size(); i++) {
            const glm::dvec3 &pi = meshes[originals[i].mesh_index].get().positions[originals[i].vertex_index];
            for (size_t j = i + 1; j < originals.size(); j++) {
                const glm::dvec3 &pj = meshes[originals[j].mesh_index].get().positions[originals[j].vertex_index];
                const double dist2 = glm::distance2(pi, pj);
                DEBUG_ASSERT(dist2 <= epsilon2);
            }
        }
    }
#endif
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
    for (size_t i = 0; i < meshes.size(); i++) {
        const SimpleMesh &mesh = meshes[i].get();
        LOG_TRACE("Mesh {}: {} vertices, {} triangles", i, mesh.vertex_count(), mesh.face_count());
    }

    std::vector<size_t> mesh_sizes;
    mesh_sizes.reserve(meshes.size());
    std::transform(meshes.begin(), meshes.end(),
                   std::back_inserter(mesh_sizes),
                   [](const auto &mesh) { return mesh.get().vertex_count(); });
    const size_t maximal_merged_mesh_size = std::accumulate(mesh_sizes.begin(), mesh_sizes.end(), 0);
    // Handle all meshes being empty
    if (maximal_merged_mesh_size == 0) {
        return {};
    }

    // Find the largest mesh and put it as the first element
    const size_t index_of_largest_mesh = std::distance(mesh_sizes.begin(), std::max_element(mesh_sizes.begin(), mesh_sizes.end()));
    std::vector<std::reference_wrapper<const SimpleMesh>> reordered;
    if (index_of_largest_mesh != 0) {
        std::copy(meshes.begin(), meshes.end(), std::back_inserter(reordered));
        std::swap(reordered[0], reordered[index_of_largest_mesh]);
        // we dont need to swap mesh_sizes here since we use the original mesh indices for the mapping
        // meshes = std::span(reordered);
    }

    VertexMapping mapping;
    mapping.init(mesh_sizes);

    size_t unique_vertices = 0;
    auto add_unique_vertex = [&](const VertexId &vertex) {
        mapping.add_bidirectional(vertex, unique_vertices);
        unique_vertices += 1;
    };

    // Init a reusable set of containers for the boundary
    std::unordered_set<glm::uvec2> boundary_edges;
    std::vector<bool> is_boundary_vertex;

    // Reusable vector for duplicate vertices
    std::vector<std::reference_wrapper<const VertexId>> duplicate_vertices;

    bool has_warned = false; // Flag to only print the intra-mesh merge warning once
    for (size_t mesh_index = 0; mesh_index < meshes.size(); mesh_index++) {
        const SimpleMesh &mesh = meshes[mesh_index];

        if (only_consider_boundary) {
            // Find boundary edges
            boundary_edges.clear();
            find_boundary_edges(mesh.triangles, boundary_edges);

            // Classify vertices as on the boundary or on the inside
            is_boundary_vertex.resize(mesh.vertex_count());
            std::fill(is_boundary_vertex.begin(), is_boundary_vertex.end(), false);
            for (const auto& edge : boundary_edges) {
                is_boundary_vertex[edge[0]] = true;
                is_boundary_vertex[edge[1]] = true;
            }
        }

        for (size_t vertex_index = 0; vertex_index < mesh.vertex_count(); vertex_index++) {
            const glm::dvec3 &position = mesh.positions[vertex_index];
            const VertexId current_vertex{
                .mesh_index = mesh_index,
                .vertex_index = vertex_index};

            if (only_consider_boundary && !is_boundary_vertex[vertex_index]) {
                add_unique_vertex(current_vertex);
            } else if (mesh_index == 0) {
                add_unique_vertex(current_vertex);
                deduplicate.add(position, current_vertex);
            } else if (!deduplicate.get_or_add(position, current_vertex, duplicate_vertices)) {
                // Duplicates detected
                std::optional<std::pair<VertexId, double>> nearest_duplicate;
                for (const auto &other_vertex : duplicate_vertices) {
                    // Warn if we would perform intra mesh merges (but dont actually do them)
                    if (other_vertex.get().mesh_index == current_vertex.mesh_index) {
                        if (!has_warned) {
                            LOG_WARN("Deduplication is too inclusive and would perform intra-mesh merges");
                            has_warned = true;
                        }
                    } else {
                        double distance2 = 0;
                        // Only caluclate the distance if there is actually more than a single duplicate vertex
                        if (duplicate_vertices.size() > 1) {
                            distance2 = glm::distance2(mesh.positions[other_vertex.get().vertex_index], position);
                        }
                        if (!nearest_duplicate.has_value() || nearest_duplicate.value().second > distance2) {
                            nearest_duplicate = {other_vertex.get(), distance2};
                        }
                    }
                }
                if (nearest_duplicate.has_value()) {
                    const auto mapped = mapping.map_forward(nearest_duplicate.value().first);
                    DEBUG_ASSERT(!mapping.map_backward(mesh_index, mapped).has_value());
                    mapping.add_bidirectional(current_vertex, mapped);
                } else {
                    deduplicate.add(position, current_vertex);
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

#ifndef NDEBUG
    using DefaultEpsilonDeduplicate = EpsilonVertexDeduplicate<3, double, VertexId, spatial_lookup::Grid3d<VertexId>>;
    const auto *epsilon_deduplicate = dynamic_cast<DefaultEpsilonDeduplicate*>(&deduplicate);
    if (epsilon_deduplicate != nullptr) {
        const double epsilon = epsilon_deduplicate->epsilon();
        validate_epsilon_mapping(mapping, meshes, epsilon);
    }
#endif

    return mapping;
}

/*
namespace {
radix::geometry::Aabb3d pad_bounds(const radix::geometry::Aabb3d &bounds, const double percentage) {
    const glm::dvec3 bounds_padding = bounds.size() * percentage;
    const radix::geometry::Aabb3d padded_bounds(bounds.min - bounds_padding, bounds.max + bounds_padding);
    return padded_bounds;
}

bool are_all_bounds_connected(std::span<std::reference_wrapper<const SimpleMesh>> meshes) {
    if (meshes.size() <= 1) {
        return true;
    }

    std::vector<radix::geometry::Aabb3d> mesh_bounds;
    mesh_bounds.reserve(meshes.size());
    std::transform(meshes.begin(), meshes.end(),
                   std::back_inserter(mesh_bounds),
                   [](const SimpleMesh &mesh) { return pad_bounds(calculate_bounds(mesh), 0.01); });
    for (size_t i = 0; i < mesh_bounds.size(); i++) {
        bool intersect_any_other = false;
        for (size_t j = 0; j < mesh_bounds.size(); j++) {
            if (i == j) {
                continue;
            }

            if (radix::geometry::intersect(mesh_bounds[i], mesh_bounds[j])) {
                intersect_any_other = true;
                break;
            }
        }
        if (!intersect_any_other) {
            LOG_WARN("Mesh at index {} is not close to any other mesh", i);
            return false;
        }
    }

    return true;
}

bool are_all_meshes_merged(const VertexMapping &mapping) {
    UnionFind union_find(mapping.mesh_count());

    const size_t maximal_merged_mesh_index = mapping.find_max_merged_index();

    std::unordered_set<size_t> observed_sources;
    observed_sources.reserve(mapping.mesh_count());
    for (size_t vertex_index = 0; vertex_index < maximal_merged_mesh_index; vertex_index++) {
        observed_sources.clear();
        for (size_t mesh_index = 0; mesh_index < mapping.mesh_count(); mesh_index++) {
            if (auto opt = mapping.map_inverse(mesh_index, vertex_index); opt.has_value()) {
                observed_sources.insert(mesh_index);
                if (observed_sources.size() > 1) {
                    for (size_t observed_source : observed_sources) {
                        if (observed_source == mesh_index) {
                            continue;
                        }

                        union_find.make_union(observed_source, mesh_index);
                    }
                }
            }
        }
    }

    return union_find.is_joint();
}

template <spatial_lookup::SpatialLookup<> Lookup>
double estimate_min_vertex_separation_between_meshes_after_merge(const Lookup &lookup, const VertexMapping &mapping) {
    double min_squared_distance = std::numeric_limits<double>::infinity();
    lookup.for_all_cells([&](const Vec& point, const Value& value) {

    });
    for (const Grid3d<VertexId>::GridCell &cell : grid.cells()) {
        for (auto first = cell.items.begin(); first != cell.items.end(); ++first) {
            for (auto second = first + 1; second != cell.items.end(); ++second) {
                const size_t mesh1 = first->value.mesh_index;
                const size_t mesh2 = second->value.mesh_index;

                if (mesh1 == mesh2) {
                    continue;
                }

                const glm::dvec3 &point1 = first->point;
                const glm::dvec3 &point2 = second->point;

                const double squared_distance = glm::distance2(point1, point2);
                min_squared_distance = std::min(min_squared_distance, squared_distance);
            }
        }
    }

    return std::sqrt(min_squared_distance);
}

double estimate_min_vertex_separation_between_meshes_after_merge(const std::span<const SimpleMesh> meshes, const VertexMapping &mapping) {
    Grid3d<VertexId> grid = construct_grid_for_meshes<VertexId>(meshes);

    for (size_t mesh_index = 0; mesh_index < meshes.size(); mesh_index++) {
        const SimpleMesh &mesh = meshes[mesh_index];
        for (size_t vertex_index = 0; vertex_index < mesh.vertex_count(); vertex_index++) {
            const glm::dvec3 &position = mesh.positions[vertex_index];
            grid.insert(position, VertexId{mesh_index, vertex_index});
        }
    }

    double min_squared_distance = std::numeric_limits<double>::infinity();
    for (const Grid3d<VertexId>::GridCell &cell : grid.cells()) {
        for (auto first = cell.items.begin(); first != cell.items.end(); ++first) {
            for (auto second = first + 1; second != cell.items.end(); ++second) {
                const size_t mesh1 = first->value.mesh_index;
                const size_t mesh2 = second->value.mesh_index;

                if (mesh1 == mesh2) {
                    // Both vertices from the same mesh
                    continue;
                }

                const size_t mapped1 = mapping.map(first->value);
                const size_t mapped2 = mapping.map(second->value);
                if (mapped1 == mapped2) {
                    // Vertices were already merged
                    continue;
                }

                const glm::dvec3 &point1 = first->point;
                const glm::dvec3 &point2 = second->point;
                const double squared_distance = glm::distance2(point1, point2);
                min_squared_distance = std::min(min_squared_distance, squared_distance);
            }
        }
    }

    return std::sqrt(min_squared_distance);
}

} // namespace

VertexMapping create_connecting_mapping(std::span<std::reference_wrapper<const SimpleMesh>> meshes) {
    LOG_DEBUG("Finding shared vertices between {} meshes (epsilon=auto)", meshes.size());

    const double inf = std::numeric_limits<double>::infinity();
    const double min_edge_length_sq = std::transform_reduce(
        meshes.begin(),
        meshes.end(),
        inf,
        [](const double a, const double b) { return std::min(a, b); },
        [](const SimpleMesh &m) { return calculate_min_edge_length_squared(m).value_or(inf); });
    if (min_edge_length_sq == inf) {
        return {};
    }
    const double min_edge_length = std::sqrt(min_edge_length_sq.value());
    DEBUG_ASSERT(min_edge_length > 0);
    double distance_epsilon = min_edge_length / 1000;
    LOG_TRACE("Starting with distance epsilon of {:g}", distance_epsilon);

    VertexMapping mapping;
    bool success = false;
    for (size_t i = 0; i < 10; i++) {
        mapping = create_mapping(meshes, distance_epsilon);

        if (are_all_meshes_merged(mapping)) {
            LOG_TRACE("Found distance epsilon that connects all meshes");

            const double min_vertex_separation_after_merge = estimate_min_vertex_separation_between_meshes_after_merge(meshes, mapping);
            if (min_vertex_separation_after_merge > min_edge_length / 2) {
                LOG_TRACE("Found vertices in merged mesh that are much closer than in the source meshes, suggesting an incomplete merge");
                continue;
            }

            success = true;
            break;
        }
        distance_epsilon *= 10;
        LOG_TRACE("Increasing distance epsilon to {:g}", distance_epsilon);
    }

    if (!success) {
        LOG_TRACE("Failed to find appropriate distance epsilon");
    }

    return mapping;
}
*/

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

    size_t max_combined_vertex_count = 0;
    size_t max_combined_face_count = 0;
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

    size_t max_vertex_index = 0;
    merged_mesh.positions.resize(max_combined_vertex_count);
    if (has_uvs) {
        merged_mesh.uvs.resize(max_combined_vertex_count);
    }
    for (size_t mesh_index = 0; mesh_index < meshes.size(); mesh_index++) {
        const SimpleMesh &mesh = meshes[mesh_index];
        for (size_t vertex_index = 0; vertex_index < mesh.vertex_count(); vertex_index++) {
            const size_t mapped_index = mapping.map_forward(VertexId{.mesh_index = mesh_index, .vertex_index = vertex_index});
            merged_mesh.positions[mapped_index] =  mesh.positions[vertex_index];
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
    for (size_t mesh_index = 0; mesh_index < meshes.size(); mesh_index++) {
        const SimpleMesh &mesh = meshes[mesh_index];
        for (size_t triangle_index = 0; triangle_index < mesh.face_count(); triangle_index++) {
            const glm::uvec3 &triangle = mesh.triangles[triangle_index];

            glm::uvec3 new_triangle;
            for (size_t k = 0; k < static_cast<size_t>(triangle.length()); k++) {
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
                    std::views::iota(0u, mesh_index),
                    [&](const size_t i) { return mapping.find_source_triangle_in_mesh(new_triangle, i).has_value(); });

                if (!is_first_mesh) {
                    LOG_WARN("Skipping duplicate triangle while merging");
                    continue;
                }
            }

            merged_mesh.triangles.push_back(new_triangle);
        }
    }

    mesh::validate(merged_mesh);

    return merged_mesh;
}
} // namespace mesh::merging

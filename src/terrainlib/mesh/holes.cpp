#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <forward_list>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "FixedVector.h"
#include "log.h"
#include "mesh/connected_components.h"
#include "mesh/holes.h"
#include "mesh/boundary.h"
#include "mesh/manifold.h"
#include "polygon/Polygon.h"
#include "polygon/triangulate.h"
#include "vector_utils.h"

namespace mesh {
    
namespace {
using Edge = glm::uvec2;
using VertexIndex = uint32_t;
using ComponentIndex = uint32_t;
}

namespace detail {
void remove_largest_boundaries(const SimpleMesh &mesh, std::vector<std::vector<VertexIndex>> &boundaries) {
    const auto& [vertex_to_component, component_count] = find_connected_components(mesh);

    // For each component, find index of largest boundary
    std::vector<std::optional<size_t>> largest_boundary_index_per_component(component_count);
    for (size_t i = 0; i < boundaries.size(); ++i) {
        const auto &boundary = boundaries[i];
        DEBUG_ASSERT(!boundary.empty());
        const ComponentIndex component_index = vertex_to_component[boundary[0]];

        auto &largest_index_opt = largest_boundary_index_per_component[component_index];
        if (!largest_index_opt.has_value() ||
            boundaries[i].size() > boundaries[largest_index_opt.value()].size()) {
            largest_index_opt = i;
        }
    }

    // Collect all indices to delete
    std::vector<size_t> to_delete;
    to_delete.reserve(component_count);
    for (auto idx_opt : largest_boundary_index_per_component) {
        if (idx_opt.has_value()) {
            to_delete.push_back(idx_opt.value());
        }
    }

    // Sort and erase from end
    std::sort(to_delete.begin(), to_delete.end());
    for (auto it = to_delete.rbegin(); it != to_delete.rend(); ++it) {
        boundaries.erase(boundaries.begin() + *it);
    }
}

std::vector<std::vector<VertexIndex>> find_holes(const SimpleMesh &mesh, const bool non_manifold) {
    std::vector<std::vector<VertexIndex>> boundaries;
    if (non_manifold) {
        boundaries = find_boundaries_non_manifold(mesh);
    } else {
        boundaries = find_boundaries(mesh);
    }
    detail::remove_largest_boundaries(mesh, boundaries);
    for (auto &boundary : boundaries) {
        // holes should be in reverse order of edges
        std::reverse(boundary.begin(), boundary.end());
    }
    return boundaries;
}
}

std::vector<std::vector<VertexIndex>> find_holes(const SimpleMesh &mesh) {
    DEBUG_ASSERT(is_manifold(mesh));
    return detail::find_holes(mesh, /* non_manifold */ false);
}

std::vector<std::vector<VertexIndex>> find_holes_non_manifold(const SimpleMesh &mesh) {
    return detail::find_holes(mesh, /* non_manifold */ true);
}

namespace {
bool is_shared_vertex(
    const VertexIndex vertex_index,
    const mesh::merging::VertexMapping &mapping) {
    bool first_source_mesh_found = false;
    for (size_t mesh_index = 0; mesh_index < mapping.mesh_count(); mesh_index++) {
        const std::optional<VertexIndex> source_vertex = mapping.map_backward(mesh_index, vertex_index);
        if (!source_vertex.has_value()) {
            continue;
        }
        if (first_source_mesh_found) {
            return true;
        }
        first_source_mesh_found = true;
    }
    return false;
}

bool contains_shared_vertex(
    const std::span<const VertexIndex> vertices_in_merged_mesh,
    const mesh::merging::VertexMapping &mapping) {
    for (const VertexIndex vertex_index : vertices_in_merged_mesh) {
        if (is_shared_vertex(vertex_index, mapping)) {
            return true;
        }
    }
    return false;
}
} // namespace

std::vector<std::vector<VertexIndex>> find_holes_on_merge_border(
    const SimpleMesh &mesh,
    const mesh::merging::VertexMapping &mapping
) {
    std::vector<std::vector<VertexIndex>> holes = find_holes_non_manifold(mesh);
    std::erase_if(holes, [&](const auto &hole) {
        return !contains_shared_vertex(hole, mapping);
    });
    return holes;
}

void fill_planar_hole(SimpleMesh &mesh, std::vector<VertexIndex> hole) {
    std::reverse(hole.begin(), hole.end());
    polygon::triangulate(mesh, hole);
}

void fill_planar_holes(SimpleMesh &mesh, std::vector<std::vector<VertexIndex>> holes) {
    for (auto& hole : holes) {
        fill_planar_hole(mesh, hole);
    }
}

void fill_holes_on_merge_border(SimpleMesh &mesh, const mesh::merging::VertexMapping &mapping) {
    const std::vector<std::vector<VertexIndex>> holes = find_holes_on_merge_border(mesh, mapping);
    fill_planar_holes(mesh, holes);
}

}

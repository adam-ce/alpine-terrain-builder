#include <cstdint>
#include <span>
#include <vector>

#include <glm/common.hpp>
#include <libassert/assert.hpp>

#include "mesh/connected_components.h"
#include "mesh/boundary.h"
#include "mesh/compute_topology.h"
#include "mesh/manifold.h"
#include "mesh/reindex.h"
#include "mesh/vertex_index_range.h"
#include "mesh/edges.h"
#include "vector_utils.h"

namespace mesh {

namespace detail {
ComponentTopology compute_component_topology(const std::span<const glm::uvec3> triangles) {
    DEBUG_ASSERT(is_single_component(triangles));
    DEBUG_ASSERT(is_manifold(triangles));
    DEBUG_ASSERT(is_orientable(triangles));

    const uint32_t vertex_count = compute_vertex_count(triangles);
    const uint32_t face_count = triangles.size();

    const std::vector<std::vector<uint32_t>> boundaries = find_boundaries(triangles);
    const uint32_t boundary_loop_count = boundaries.size();
    const uint32_t boundary_edge_count = sum(boundaries, [&](const std::vector<uint32_t> &boundary) {
        return boundary.size();
    });

    ComponentTopology c = ComponentTopology::create(vertex_count, face_count, boundary_loop_count, boundary_edge_count);
    DEBUG_ASSERT(c.edge_count() == compute_edge_count(triangles));
    DEBUG_ASSERT(c.halfedge_count() == compute_halfedge_count(triangles));
    return c;
}
}

Topology compute_topology(const std::span<const glm::uvec3> triangles) {
    std::vector<ComponentTopology> ts;
    auto components = split_into_connected_components(triangles);
    for (std::vector<glm::uvec3>& component_triangles : components) {
        reindex_inplace(component_triangles);
        ts.push_back(detail::compute_component_topology(component_triangles));
    }

    return Topology::create(ts);
}

}

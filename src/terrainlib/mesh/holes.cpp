#include <forward_list>

#include <glm/gtx/hash.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "mesh/holes.h"
#include "FixedVector.h"
#include "mesh/utils.h"
#include "mesh/connected_components.h"
#include "log.h"
#include "polygon/Polygon.h"
#include "polygon/triangulate.h"

namespace {
using Edge = glm::uvec2;
using VertexIndex = uint32_t;
using ComponentIndex = uint32_t;
} // namespace

namespace mesh {
namespace {
template <typename T>
void remove_first(std::vector<T> &vec, const T &value) {
    auto it = std::find(vec.begin(), vec.end(), value);
    if (it != vec.end()) {
        vec.erase(it);
    }
}
}

std::vector<std::vector<VertexIndex>> find_boundaries(const SimpleMesh &mesh) {
    std::unordered_set<Edge> boundary_edges = find_boundary_edges(mesh);
    std::unordered_map<VertexIndex, std::vector<VertexIndex>> adjacencies;
    adjacencies.reserve((boundary_edges.size() * 3) / 2);
    for (const Edge &edge : boundary_edges) {
        adjacencies[edge[0]].push_back(edge[1]);
    }

    auto remove_edge = [&](const auto edge) {
        boundary_edges.erase(edge);
        remove_first(adjacencies[edge[0]], edge[1]);
    };

    std::vector<std::vector<VertexIndex>> boundaries;
    while (!boundary_edges.empty()) {
        const Edge starting_edge = *boundary_edges.begin();
        remove_edge(starting_edge);

        std::vector<VertexIndex> boundary;
        const VertexIndex starting_vertex_id = starting_edge[0];

        Edge current_edge = starting_edge;
        VertexIndex current_vertex_id = starting_edge[1];
        boundary.push_back(current_vertex_id);
        while (true) {
            const auto neighbours = adjacencies[current_vertex_id];
            if (neighbours.empty()) {
                // non-manifold
                break;
            }
            const VertexIndex next_vertex_id = neighbours[0];
            const Edge next_edge(current_vertex_id, next_vertex_id);
            remove_edge(next_edge);

            boundary.push_back(next_vertex_id);
            if (next_vertex_id == starting_vertex_id) {
                break;
            }
            current_vertex_id = next_vertex_id;
            current_edge = next_edge;
        }

        if (boundary.empty()) {
            continue;
        }

        std::reverse(boundary.begin(), boundary.end());

        // split boundary into individual loops
        std::unordered_map<VertexIndex, size_t> visited;
        for (size_t i = 0; i < boundary.size(); i++) {
            const VertexIndex vertex = boundary[i];
            auto it = visited.find(vertex);
            if (it != visited.end()) {
                const size_t first_occurance = it->second;
                // finished current loop
                DEBUG_ASSERT(first_occurance < i);
                auto loop_start = boundary.begin() + first_occurance;
                auto loop_end = boundary.begin() + i;
                std::vector<VertexIndex> loop(loop_start, loop_end);
                boundaries.push_back(std::move(loop));
                boundary.erase(loop_start, loop_end);
                i = first_occurance;
            } else {
                visited.emplace(vertex, i);
            }
        }

        if (boundary.empty()) {
            continue;
        }

        boundaries.push_back(std::move(boundary));
    }

    return boundaries;
}

namespace {
template <typename T>
auto iterator_from_ref(std::vector<T> &vec, T &ref) {
    // Make sure ref actually belongs to vec
    DEBUG_ASSERT(&ref >= vec.data() && &ref < vec.data() + vec.size());
    return vec.begin() + (&ref - vec.data());
}

template <typename T>
auto iterator_from_ref(const std::vector<T> &vec, const T &ref) {
    DEBUG_ASSERT(&ref >= vec.data() && &ref < vec.data() + vec.size());
    return vec.cbegin() + (&ref - vec.data());
}
}

std::vector<std::vector<VertexIndex>> find_holes(const SimpleMesh &mesh) {
    std::vector<std::vector<VertexIndex>> boundaries = find_boundaries(mesh);
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

    return boundaries;
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
    std::vector<std::vector<VertexIndex>> holes = find_holes(mesh);
    for (auto it = holes.rbegin(); it != holes.rend(); it++) {
        const auto &hole = *it;

        if (!contains_shared_vertex(hole, mapping)) {
            // Not a hole between meshes
            holes.erase((it + 1).base());
        }
    }
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

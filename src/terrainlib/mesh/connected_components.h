#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "UnionFind.h"
#include "mesh/SimpleMesh.h"

namespace {
using VertexIndex = uint32_t;
using ComponentIndex = uint32_t;
} // namespace

namespace mesh {
struct ComponentsIndex {
    std::vector<ComponentIndex> vertex_to_component;
    size_t component_count;
};

namespace detail {
inline UnionFind build_union_find(const std::span<const glm::uvec3> &triangles, const size_t vertex_count) {
    UnionFind components(vertex_count);
    for (const glm::uvec3 &triangle : triangles) {
        components.make_union(triangle[0], triangle[1]);
        components.make_union(triangle[1], triangle[2]);
    }
    return components;
}
}

inline ComponentsIndex find_connected_components(const std::span<const glm::uvec3> &triangles, const size_t vertex_count) {
    if (vertex_count == 0) {
        return {};
    }

    // Union vertices that belong to the same triangle
    UnionFind components = detail::build_union_find(triangles, vertex_count);

    // Convert to vertex to component index mapping
    std::unordered_map<VertexIndex, ComponentIndex> rep_to_index;
    ComponentIndex next_index = 0;

    std::vector<ComponentIndex> vertex_to_component(vertex_count);
    for (VertexIndex vertex = 0; vertex < vertex_count; vertex++) {
        const VertexIndex rep = components.find(vertex);
        auto it = rep_to_index.find(rep);
        if (it == rep_to_index.end()) {
            rep_to_index[rep] = next_index;
            vertex_to_component[vertex] = next_index;
            next_index += 1;
        } else {
            vertex_to_component[vertex] = it->second;
        }
    }

    return ComponentsIndex{
        .vertex_to_component = vertex_to_component,
        .component_count = rep_to_index.size()};
}
template <glm::length_t n_dims, typename T>
ComponentsIndex find_connected_components(const mesh::View_<n_dims, T> &mesh) {
    return find_connected_components(mesh.triangles, mesh.vertex_count());
}
template <glm::length_t n_dims, typename T>
ComponentsIndex find_connected_components(const mesh::Simple_<n_dims, T> &mesh) {
    return find_connected_components(mesh.triangles, mesh.vertex_count());
}

template <glm::length_t n_dims, typename T>
std::vector<mesh::Simple_<n_dims, T>> split_into_connected_components(const mesh::View_<n_dims, T> &mesh, const ComponentsIndex& components_index) {
    const auto &[vertex_to_component, component_count] = components_index;

    std::vector<mesh::Simple_<n_dims, T>> components;
    components.resize(component_count);

    constexpr VertexIndex invalid_index = std::numeric_limits<VertexIndex>::max();
    std::vector<VertexIndex> old_to_new(mesh.vertex_count(), invalid_index);
    for (VertexIndex vertex = 0; vertex < mesh.vertex_count(); vertex++) {
        const ComponentIndex component_index = vertex_to_component[vertex];
        mesh::Simple_<n_dims, T> &component = components[component_index];
        old_to_new[vertex] = component.positions.size();
        component.positions.push_back(mesh.positions[vertex]);
    }

    for (mesh::Simple_<n_dims, T> &component : components) {
        component.triangles.reserve((component.positions.size() * 3) / 2);
    }

    for (const glm::uvec3 &triangle : mesh.triangles) {
        const ComponentIndex component_index = vertex_to_component[triangle[0]];

        glm::uvec3 new_triangle;
        bool skip_triangle = false;
        for (size_t k = 0; k < static_cast<size_t>(triangle.length()); k++) {
            if (k > 0 && vertex_to_component[triangle[k]] != component_index) {
                skip_triangle = true;
                break;
            }
            new_triangle[k] = old_to_new[triangle[k]];
        }
        if (skip_triangle) {
            continue;
        }

        mesh::Simple_<n_dims, T> &component = components[component_index];
        component.triangles.push_back(new_triangle);
    }

    return components;
}
template <glm::length_t n_dims, typename T>
std::vector<mesh::Simple_<n_dims, T>> split_into_connected_components(const mesh::View_<n_dims, T> &mesh) {
    const ComponentsIndex& components_index = find_connected_components(mesh);
    return split_into_connected_components(mesh, components_index);
}
template <glm::length_t n_dims, typename T>
std::vector<mesh::Simple_<n_dims, T>> split_into_connected_components(const mesh::Simple_<n_dims, T> &mesh) {
    return split_into_connected_components(mesh::View_<n_dims, T>(mesh));
}

inline size_t count_connected_components(const std::span<const glm::uvec3> triangles, const size_t vertex_count) {
    if (vertex_count == 0 || triangles.empty()) {
        return 0;
    }

    UnionFind components = detail::build_union_find(triangles, vertex_count);
    return components.set_count();
}
template <glm::length_t n_dims, typename T>
size_t count_connected_components(const mesh::View_<n_dims, T> &mesh) {
    return count_connected_components(mesh.triangles, mesh.vertex_count());
}
template <glm::length_t n_dims, typename T>
size_t count_connected_components(const mesh::Simple_<n_dims, T> &mesh) {
    return count_connected_components(mesh.triangles, mesh.vertex_count());
}

inline bool is_single_component(const std::span<const glm::uvec3> &triangles, const size_t vertex_count) {
    return count_connected_components(triangles, vertex_count) == 1u;
}
template <glm::length_t n_dims, typename T>
bool is_single_component(const mesh::View_<n_dims, T> &mesh) {
    return is_single_component(mesh.triangles, mesh.vertex_count());
}
template <glm::length_t n_dims, typename T>
bool is_single_component(const mesh::Simple_<n_dims, T> &mesh) {
    return is_single_component(mesh.triangles, mesh.vertex_count());
}

} // namespace mesh

#pragma once

#include <optional>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "mesh/merging/VertexId.h"

namespace mesh::merging {

// Represents a triangle from a specific mesh.
struct TriangleInMesh {
    size_t mesh_index;
    glm::uvec3 triangle;
};

// Manages mapping between the vertices of multiple original meshes and the corresponding vertices in a merged mesh.
class VertexMapping {
public:
    // Creates a mapping where every vertex maps to itself (identity mapping) for a single mesh.
    static VertexMapping identity(const size_t vertex_count) {
        VertexMapping mapping;
        mapping.init({&vertex_count, 1});
        for (size_t i = 0; i < vertex_count; i++) {
            mapping.add_bidirectional(VertexId{.mesh_index = 0, .vertex_index = i}, i);
        }
        return mapping;
    }

    // Initializes the mapping structures based on per-mesh vertex counts.
    void init(std::span<const size_t> vertex_counts) {
        this->forward.resize(vertex_counts.size());
        for (size_t i = 0; i < vertex_counts.size(); i++) {
            this->forward[i].resize(vertex_counts[i]);
        }

        this->backward.resize(vertex_counts.size());
        for (size_t i = 0; i < vertex_counts.size(); i++) {
            this->backward[i].reserve(vertex_counts[i]);
        }
    }

    // Adds a forward and backward mapping between a source vertex and a merged index.
    void add_bidirectional(VertexId source, size_t mapped) {
        this->add_forward(source, mapped);
        this->add_backward(source, mapped);
    }

    void add_forward(VertexId source, size_t mapped) {
        this->forward[source.mesh_index][source.vertex_index] = mapped;
    }

    void add_backward(VertexId source, size_t mapped) {
        this->backward[source.mesh_index][mapped] = source.vertex_index;
    }

    // Returns the merged vertex index for a given source vertex.
    size_t map_forward(VertexId source) const {
        return this->forward.at(source.mesh_index).at(source.vertex_index);
    }

    // Returns the original vertex index for a given merged index and mesh, if it exists.
    std::optional<size_t> map_backward(size_t mesh_index, size_t mapped_index) const {
        const auto it = this->backward.at(mesh_index).find(mapped_index);
        if (it != this->backward.at(mesh_index).end()) {
            return it->second;
        }
        return std::nullopt;
    }

    // Returns a list of meshes in which a given merged index exists.
    std::vector<size_t> find_source_meshes(size_t mapped_index) const {
        std::vector<size_t> exists;
        exists.reserve(this->mesh_count());
        for (size_t mesh_index = 0; mesh_index < this->mesh_count(); mesh_index++) {
            if (this->map_backward(mesh_index, mapped_index)) {
                exists.push_back(mesh_index);
            }
        }
        return exists;
    }

    // Searches all meshes to find a triangle whose vertices map to the given merged triangle.
    TriangleInMesh find_source_triangle(glm::uvec3 mapped_triangle) const {
        for (size_t mesh_index = 0; mesh_index < this->mesh_count(); mesh_index++) {
            const std::optional<glm::uvec3> source_triangle_opt = this->find_source_triangle_in_mesh(mapped_triangle, mesh_index);
            if (source_triangle_opt.has_value()) {
                const glm::uvec3 source_triangle = source_triangle_opt.value();
                return TriangleInMesh { .mesh_index=mesh_index, .triangle=source_triangle };
            }
        }

        throw std::runtime_error("illegal state in vertex mapping");
    }

    // Attempts to find the source triangle (in a specific mesh) corresponding to the given mapped triangle.
    std::optional<glm::uvec3> find_source_triangle_in_mesh(glm::uvec3 mapped_triangle, size_t mesh_index) const {
        glm::uvec3 source_triangle;

        for (size_t i = 0; i < static_cast<size_t>(mapped_triangle.length()); i++) {
            const size_t mapped_vertex_index = mapped_triangle[i];
            const std::optional<size_t> source_vertex = this->map_backward(mesh_index, mapped_vertex_index);
            if (source_vertex.has_value()) {
                source_triangle[i] = source_vertex.value();
            } else {
                return std::nullopt;
            }

            DEBUG_ASSERT(this->map_forward(VertexId { .mesh_index = mesh_index, .vertex_index = source_vertex.value() }) == mapped_vertex_index);
        }

        return source_triangle;
    }

    // Returns the number of meshes tracked by this mapping.
    size_t mesh_count() const {
        return this->backward.size();
    }

    // Finds the maximum vertex index in the merged mesh.
    size_t find_max_merged_index() const {
        size_t max_index = 0;
        for (size_t i = 0; i < this->mesh_count(); i++) {
            if (this->forward[i].empty()) {
                continue;
            }
            max_index = std::max(max_index, *std::max_element(this->forward[i].begin(), this->forward[i].end()));
        }
        return max_index;
    }

    // Returns the number of vertices in a specific source mesh.
    size_t mesh_vertex_count(const size_t mesh_index) const {
        return this->forward[mesh_index].size();
    }

    // Performs internal consistency checks in debug mode.
    void validate() const {
#ifndef NDEBUG
        for (size_t i = 0; i < this->mesh_count(); i++) {
            DEBUG_ASSERT(this->forward[i].size() == this->backward[i].size());

            for (size_t j = 0; j < this->forward[i].size(); j++) {
                const size_t mapped = this->map_forward(VertexId { .mesh_index = i, .vertex_index = j });
                const std::optional<size_t> inv_mapped = this->map_backward(i, mapped);
                DEBUG_ASSERT(inv_mapped.has_value());
                DEBUG_ASSERT(inv_mapped.value() == j);
            }

            for (const std::pair<unsigned int, unsigned int> e : this->backward[i]) {
                const std::optional<size_t> inv_mapped = this->map_backward(i, e.first);
                DEBUG_ASSERT(inv_mapped.has_value());
                const size_t mapped = this->map_forward(VertexId{.mesh_index = i, .vertex_index = inv_mapped.value()});
                DEBUG_ASSERT(mapped == e.first);
            }
        }
#endif
    }

private:
    // TODO: backward mapping could be a single unordered_map

    // Forward mapping: [mesh_index][vertex_index] -> merged index.
    std::vector<std::vector<size_t>> forward;
    // Backward mapping: [mesh_index][merged index] -> original vertex index.
    std::vector<std::unordered_map<size_t, size_t>> backward;
};

}

#pragma once

#include <unordered_map>
#include <span>
#include <optional>
#include <vector>

#include "pch.h"

namespace mesh::merge {

struct VertexId {
    size_t mesh_index;
    size_t vertex_index;
};

struct TriangleInMesh {
    size_t mesh_index;
    glm::uvec3 triangle;
};

class VertexMapping {
public:
    static VertexMapping identity(const size_t vertex_count) {
        VertexMapping mapping;
        mapping.init({&vertex_count, 1});
        for (size_t i = 0; i < vertex_count; i++) {
            mapping.add_bidirectional(VertexId{.mesh_index = 0, .vertex_index = i}, i);
        }
        return mapping;
    }

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

    size_t map(VertexId source) const {
        return this->forward.at(source.mesh_index).at(source.vertex_index);
    }

    std::optional<size_t> map_inverse(size_t mesh_index, size_t mapped_index) const {
        const auto it = this->backward.at(mesh_index).find(mapped_index);
        if (it != this->backward.at(mesh_index).end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::vector<size_t> map_inverse_exists(size_t mapped_index) const {
        std::vector<size_t> exists;
        exists.reserve(this->mesh_count());
        for (size_t mesh_index = 0; mesh_index < this->mesh_count(); mesh_index++) {
            if (this->map_inverse(mapped_index, mesh_index)) {
                exists.push_back(mesh_index);
            }
        }
        return exists;
    }

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

    std::optional<glm::uvec3> find_source_triangle_in_mesh(glm::uvec3 mapped_triangle, size_t mesh_index) const {
        glm::uvec3 source_triangle;

        for (size_t i = 0; i < static_cast<size_t>(mapped_triangle.length()); i++) {
            const size_t mapped_vertex_index = mapped_triangle[i];
            const std::optional<size_t> source_vertex = this->map_inverse(mesh_index, mapped_vertex_index);
            if (source_vertex.has_value()) {
                source_triangle[i] = source_vertex.value();
            } else {
                return std::nullopt;
            }

            DEBUG_ASSERT(this->map(VertexId { .mesh_index = mesh_index, .vertex_index = source_vertex.value() }) == mapped_vertex_index);
        }

        return source_triangle;
    }

    size_t mesh_count() const {
        return this->backward.size();
    }

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

    size_t mesh_vertex_count(const size_t mesh_index) const {
        return this->forward[mesh_index].size();
    }

    void validate() const {
        for (size_t i = 0; i < this->mesh_count(); i++) {
            DEBUG_ASSERT(this->forward[i].size() == this->backward[i].size());

            for (size_t j = 0; j < this->forward[i].size(); j++) {
                const size_t mapped = this->map(VertexId { .mesh_index = i, .vertex_index = j });
                const std::optional<size_t> inv_mapped = this->map_inverse(i, mapped);
                DEBUG_ASSERT(inv_mapped.has_value());
                DEBUG_ASSERT(inv_mapped.value() == j);
            }

            for (const std::pair<unsigned int, unsigned int> e : this->backward[i]) {
                const std::optional<size_t> inv_mapped = this->map_inverse(i, e.first);
                DEBUG_ASSERT(inv_mapped.has_value());
                const size_t mapped = this->map(VertexId{.mesh_index = i, .vertex_index = inv_mapped.value()});
                DEBUG_ASSERT(mapped == e.first);
            }
        }
    }

    private:
        std::vector<std::vector<size_t>> forward;
        std::vector<std::unordered_map<size_t, size_t>> backward;
};

// TODO: fix namespace and naming
// TODO: accept refs
SimpleMesh merge_meshes(std::span<const SimpleMesh> meshes);
SimpleMesh merge_meshes(std::span<const SimpleMesh> meshes, VertexMapping &mapping);
SimpleMesh merge_meshes(std::span<const SimpleMesh> meshes, double distance_epsilon);
SimpleMesh merge_meshes(std::span<const SimpleMesh> meshes, double distance_epsilon, VertexMapping &mapping);

SimpleMesh apply_mapping(std::span<const SimpleMesh> meshes, const VertexMapping &mapping);

VertexMapping create_merge_mapping(std::span<const SimpleMesh> meshes);
VertexMapping create_merge_mapping(std::span<const SimpleMesh> meshes, double distance_epsilon);

// TODO:
inline SimpleMesh merge_meshes(const SimpleMesh &mesh1, const SimpleMesh &mesh2) {
    std::array<const SimpleMesh, 2> meshes = {mesh1, mesh2};
    return merge_meshes(meshes);
}
inline SimpleMesh merge_meshes(const SimpleMesh& mesh1, const SimpleMesh& mesh2, const SimpleMesh& mesh3) {
    std::array<const SimpleMesh, 3> meshes = {mesh1, mesh2, mesh3};
    return merge_meshes(meshes);
}

}


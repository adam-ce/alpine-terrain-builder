#pragma once

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <vector>
#include <span>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "build_config.h"
#include "containers/SegmentedBuffer.h"
#include "containers/HybridIndexPairMap.h"
#include "mesh/merging/VertexId.h"
#include "log.h"

namespace mesh::merging {

// Represents a triangle from a specific mesh.
struct TriangleInMesh {
    uint32_t mesh_index;
    glm::uvec3 triangle;
};

// Manages mapping between the vertices of multiple meshes and the corresponding vertices in a merged mesh.
class VertexMapping {
public:
    // Creates a mapping where every vertex maps to itself for a single mesh.
    static VertexMapping identity(const uint32_t vertex_count) {
        VertexMapping mapping;
        mapping.init({&vertex_count, 1});
        for (uint32_t i = 0; i < vertex_count; i++) {
            mapping.add(VertexId{.mesh_index = 0, .vertex_index = i}, i);
        }
        return mapping;
    }

    // Initializes the mapping structures based on per-mesh vertex counts.
    void init(const std::span<const uint32_t> vertex_counts) {
        this->_forward.init(vertex_counts);

        this->_backward.clear();
        this->_backward.reserve_primary(this->_forward.total_size());
    }

    // Adds a forward and backward mapping between a source vertex and a merged index.
    void add(const VertexId source, const uint32_t mapped) {
        this->add_forward(source, mapped);
        this->add_backward(source, mapped);
    }

    // Returns the merged vertex index for a given source vertex.
    uint32_t map_forward(const VertexId source) const {
        return this->map_forward(source.mesh_index, source.vertex_index);
    }
    // Returns the merged vertex index for a given source vertex.
    uint32_t map_forward(const uint32_t mesh_index, const uint32_t vertex_index) const {
        return this->_forward(mesh_index, vertex_index);
    }

    // Returns the original vertex index for a given merged index and mesh, if it exists.
    std::optional<uint32_t> map_backward(const uint32_t mesh_index, const uint32_t mapped_index) const {
        return this->_backward.find(mapped_index, mesh_index);
    }

    // Returns a list of meshes in which a given merged index exists.
    std::vector<uint32_t> find_source_meshes(const uint32_t mapped_index) const {
        std::vector<uint32_t> source_meshes;
        this->find_source_meshes(mapped_index, source_meshes);
        return source_meshes;
    }
    // Fills the given vector with a list of source meshes for which the given mapped vertex exists.
    size_t find_source_meshes(const uint32_t mapped_index, std::vector<uint32_t>& source_meshes) const {
        source_meshes.clear();
        for (uint32_t mesh_index = 0; mesh_index < this->mesh_count(); mesh_index++) {
            if (this->map_backward(mesh_index, mapped_index)) {
                source_meshes.push_back(mesh_index);
            }
        }
        DEBUG_ASSERT(!source_meshes.empty());
        return source_meshes.size();
    }
    // Returns a list of source vertices for the given mapped vertex.
    std::vector<VertexId> find_source_vertices(const uint32_t mapped_index) const {
        std::vector<VertexId> source_vertices;
        this->find_source_vertices(mapped_index, source_vertices);
        return source_vertices;
    }
    // Fills the given vector with a list of source vertices for the given mapped vertex.
    size_t find_source_vertices(const uint32_t mapped_index, std::vector<VertexId> &source_vertices) const {
        source_vertices.clear();
        for (uint32_t mesh_index = 0; mesh_index < this->mesh_count(); mesh_index++) {
            if (const auto vertex_index = this->map_backward(mesh_index, mapped_index)) {
                source_vertices.push_back(VertexId{mesh_index, *vertex_index});
            }
        }
        DEBUG_ASSERT(!source_vertices.empty());
        return source_vertices.size();
    }

    // Searches all meshes to find a triangle whose vertices map to the given merged triangle.
    TriangleInMesh find_source_triangle(const glm::uvec3 mapped_triangle) const {
        for (uint32_t mesh_index = 0; mesh_index < this->mesh_count(); mesh_index++) {
            const std::optional<glm::uvec3> source_triangle_opt = this->find_source_triangle_in_mesh(mapped_triangle, mesh_index);
            if (source_triangle_opt.has_value()) {
                const glm::uvec3 source_triangle = source_triangle_opt.value();
                return TriangleInMesh{.mesh_index = mesh_index, .triangle = source_triangle};
            }
        }

        UNREACHABLE();
    }

    // Attempts to find the source triangle in a specific mesh corresponding to the given mapped triangle.
    std::optional<glm::uvec3> find_source_triangle_in_mesh(const glm::uvec3 mapped_triangle, const uint32_t mesh_index) const {
        glm::uvec3 source_triangle;

        for (uint32_t i = 0; i < static_cast<uint32_t>(mapped_triangle.length()); i++) {
            const uint32_t mapped_vertex_index = mapped_triangle[i];
            const std::optional<uint32_t> source_vertex = this->map_backward(mesh_index, mapped_vertex_index);
            if (source_vertex.has_value()) {
                source_triangle[i] = source_vertex.value();
            } else {
                return std::nullopt;
            }

            DEBUG_ASSERT(this->map_forward(VertexId{.mesh_index = mesh_index, .vertex_index = source_vertex.value()}) == mapped_vertex_index);
        }

        return source_triangle;
    }

    // Returns the number of meshes tracked by this mapping.
    uint32_t mesh_count() const {
        return this->_forward.segment_count();
    }

    bool empty() const {
        return this->_forward.total_size() == 0;
    }

    // Finds the maximum vertex index in the merged mesh.
    uint32_t find_max_merged_index() const {
        DEBUG_ASSERT(!this->empty());
        const std::span<const uint32_t> mapped_indices = this->_forward.flat();
        return *std::max_element(mapped_indices.begin(), mapped_indices.end());
    }

    // Finds the maximum vertex index in the merged mesh.
    uint32_t merged_vertex_count() const {
        if (this->empty()) {
            return 0;
        }
        return this->find_max_merged_index() + 1;
    }

    // Returns the number of vertices in a specific source mesh.
    uint32_t mesh_vertex_count(const uint32_t mesh_index) const {
        return this->_forward.segment_size(mesh_index);
    }

    // Performs internal consistency checks in debug mode.
    void validate() const {
        if constexpr (IS_DEBUG_BUILD) {
            // Check backward(forward(x)) == x
            for (uint32_t i = 0; i < this->mesh_count(); i++) {
                const uint32_t vertex_count = this->mesh_vertex_count(i);
                for (uint32_t j = 0; j < vertex_count; j++) {
                    const uint32_t mapped = this->map_forward(VertexId{.mesh_index = i, .vertex_index = j});
                    const std::optional<uint32_t> inv_mapped = this->map_backward(i, mapped);
                    DEBUG_ASSERT(inv_mapped.has_value());
                    DEBUG_ASSERT(inv_mapped.value() == j);
                    ALP_UNUSED(inv_mapped);
                }
            }

            // Check forward(backward(x)) == x
            for (const auto [merged_vertex_index, source_mesh_index, source_vertex_index] : this->_backward.entries()) {
                DEBUG_ASSERT(source_mesh_index < this->mesh_count());
                DEBUG_ASSERT(source_vertex_index < this->mesh_vertex_count(source_mesh_index));
                DEBUG_ASSERT(this->map_forward(VertexId{.mesh_index = source_mesh_index, .vertex_index = source_vertex_index}) == merged_vertex_index);
                ALP_UNUSED(merged_vertex_index);
                ALP_UNUSED(source_mesh_index);
                ALP_UNUSED(source_vertex_index);
            }

            // Check forward(x) != forward(y)
            for (uint32_t i = 0; i < this->mesh_count(); i++) {
                const uint32_t vertex_count = this->mesh_vertex_count(i);
                for (uint32_t j = 0; j < vertex_count; j++) {
                    const uint32_t mapped_j = this->map_forward(VertexId{.mesh_index = i, .vertex_index = j});
                    for (uint32_t k = j + 1; k < vertex_count; k++) {
                        const uint32_t mapped_k = this->map_forward(VertexId{.mesh_index = i, .vertex_index = k});
                        DEBUG_ASSERT(mapped_j != mapped_k);
                        ALP_UNUSED(mapped_k);
                    }
                    ALP_UNUSED(mapped_j);
                }
            }

            DEBUG_ASSERT(this->_backward.size() == this->_forward.total_size());
        }
    }

    auto into_parts() && {
        return std::make_tuple(
            std::move(this->_forward),
            std::move(this->_backward));
    }

private:
    void add_forward(const VertexId source, const uint32_t mapped_index) {
        this->_forward(source.mesh_index, source.vertex_index) = mapped_index;
    }

    void add_backward(const VertexId source, const uint32_t mapped_index) {
        this->_backward.insert_or_assign(mapped_index, source.mesh_index, source.vertex_index);
    }

    SegmentedBuffer<uint32_t, uint32_t> _forward;
    HybridIndexPairMap<uint32_t, uint32_t> _backward;
};

} // namespace mesh::merging

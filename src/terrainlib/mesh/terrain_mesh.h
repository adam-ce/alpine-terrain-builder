#ifndef TERRAINMESH_H
#define TERRAINMESH_H

#include <vector>
#include <optional>
#include <span>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>
#include <radix/geometry.h>
#include <glm/gtx/hash.hpp>
#include <zpp_bits.h>


class TerrainMesh {
public:
    using Triangle = glm::uvec3;
    using Edge = glm::uvec2;
    using Position = glm::dvec3;
    using Uv = glm::dvec2;
    using Texture = cv::Mat;
    using serialize = zpp::bits::members<4>;

    TerrainMesh(std::vector<Triangle> triangles, std::vector<Position> positions) 
        : TerrainMesh(triangles, positions, {}) {}
    TerrainMesh(std::vector<Triangle> triangles, std::vector<Position> positions, std::vector<Uv> uvs) 
        : TerrainMesh(triangles, positions, uvs, std::nullopt) {}
    TerrainMesh(std::vector<Triangle> triangles, std::vector<Position> positions, std::vector<Uv> uvs, Texture texture)
        : TerrainMesh(triangles, positions, uvs, std::optional<Texture>(std::move(texture))) {}
    TerrainMesh(std::vector<Triangle> triangles, std::vector<Position> positions, std::vector<Uv> uvs, std::optional<Texture> texture)
        : triangles(triangles), positions(positions), uvs(uvs), texture(std::move(texture)) {}
    TerrainMesh() = default;
    TerrainMesh(TerrainMesh &&) = default;
    TerrainMesh &operator=(TerrainMesh &&) = default;
    TerrainMesh(const TerrainMesh &) = default;
    TerrainMesh &operator=(const TerrainMesh &) = default;

    std::vector<Triangle> triangles;
    std::vector<Position> positions;
    std::vector<Uv> uvs;
    std::optional<Texture> texture;

    size_t vertex_count() const {
        return this->positions.size();
    }
    size_t face_count() const {
        return this->triangles.size();
    }

    bool has_uvs() const {
        return this->positions.size() == this->uvs.size();
    }
    bool has_texture() const {
        return this->texture.has_value();
    }
};

radix::geometry::Aabb<3, double> calculate_bounds(const TerrainMesh &mesh);
radix::geometry::Aabb<3, double> calculate_bounds(std::span<const TerrainMesh> meshes);

std::optional<double> estimate_average_edge_length(const TerrainMesh &mesh, const size_t sample_size = 1000);
std::optional<double> calculate_max_edge_length(const TerrainMesh &mesh);

std::vector<size_t> find_isolated_vertices(const TerrainMesh &mesh);
size_t remove_isolated_vertices(TerrainMesh & mesh);
size_t remove_triangles_of_negligible_size(TerrainMesh & mesh, const double threshold_percentage_of_average = 0.001);

bool compare_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2);
bool compare_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2);
bool compare_equality_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2);
bool compare_equality_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2);

template <typename Triangles>
inline auto find_duplicate_triangles(Triangles& triangles, bool ignore_orientation = true) {
    std::sort(std::begin(triangles), std::end(triangles), ignore_orientation ? compare_triangles_ignore_orientation : compare_triangles);
    return std::unique(std::begin(triangles), std::end(triangles), ignore_orientation ? compare_equality_triangles_ignore_orientation : compare_equality_triangles);
}
template <>
inline auto find_duplicate_triangles(TerrainMesh& mesh, bool ignore_orientation) {
    return find_duplicate_triangles(mesh.triangles, ignore_orientation);
}
void remove_duplicate_triangles(TerrainMesh& mesh, bool ignore_orientation = true);
void remove_duplicate_triangles(std::vector<glm::uvec3>& triangles, bool ignore_orientation = true);

std::unordered_map<glm::uvec2, std::vector<size_t>> create_edge_to_triangle_index_mapping(const TerrainMesh &mesh);
std::vector<size_t> count_vertex_adjacent_triangles(const TerrainMesh &mesh);

std::vector<glm::uvec2> find_non_manifold_edges(const TerrainMesh &mesh);
std::vector<size_t> find_single_non_manifold_triangle_indices(const TerrainMesh &mesh);
void remove_single_non_manifold_triangles(TerrainMesh& mesh);

void sort_and_normalize_triangles(TerrainMesh& mesh);
void sort_and_normalize_triangles(std::span<glm::uvec3> triangles);

void validate_mesh(const TerrainMesh &mesh);

#endif

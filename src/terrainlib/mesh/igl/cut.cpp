#include <span>

#include <Eigen/Core>
#include <igl/cut_mesh.h>
#include <glm/common.hpp>

#include <libassert/assert.hpp>

#include "mesh/igl/convert.h"
#include "mesh/igl/cut.h"
#include "mesh/topology/vertex_index_range.h"

namespace mesh {

namespace detail {
inline Eigen::VectorX<uint8_t> create_dummy_vertices(const std::span<const glm::uvec3> triangles) {
    const uint32_t max_vertex_index = mesh::find_max_vertex_index(triangles);
    return Eigen::VectorX<uint8_t>::Zero(max_vertex_index + 1);
}
}

std::vector<uint32_t> cut(
    std::span<glm::uvec3> triangles,
    const std::span<const glm::bvec3> edge_cut_mask) {
    auto F = convert_triangles(triangles);
    auto V = detail::create_dummy_vertices(triangles);
    const auto C = to_eigen_matrix(edge_cut_mask);
    Eigen::VectorXi I;
    igl::cut_mesh(V, F, C, I);
    to_stl_glm_span(F, triangles);
    return to_stl_vector<decltype(I), uint32_t>(I);
}

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> cut(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, T>> &positions,
    const std::span<const glm::bvec3> edge_cut_mask) {
    auto F = convert_triangles(triangles);
    auto V = to_eigen_matrix(positions);
    const auto C = to_eigen_matrix(edge_cut_mask);
    Eigen::VectorXi I;
    igl::cut_mesh(V, F, C, I);
    to_stl_glm_span(F, triangles);
    to_stl_glm_vector(V, positions);
    return to_stl_vector<decltype(I), uint32_t>(I);
}

template std::vector<uint32_t> cut<3, double>(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<3, double>> &positions,
    std::span<const glm::bvec3> edge_cut_mask);

template std::vector<uint32_t> cut<1, uint32_t>(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<1, uint32_t>> &positions,
    std::span<const glm::bvec3> edge_cut_mask);
}

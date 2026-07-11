#include <vector>
#include <span>

#include <glm/common.hpp>
#include <igl/is_edge_manifold.h>
#include <igl/is_vertex_manifold.h>
#include <igl/split_nonmanifold.h>

#include "mesh/igl/convert.h"

namespace mesh::igl {

bool is_manifold(const std::span<const glm::uvec3> triangles) {
    auto F = convert_triangles(triangles);
    return ::igl::is_edge_manifold(F) && ::igl::is_vertex_manifold(F);
}
bool is_edge_manifold(const std::span<const glm::uvec3> triangles) {
    auto F = convert_triangles(triangles);
    return ::igl::is_edge_manifold(F);
}
bool is_vertex_manifold(const std::span<const glm::uvec3> triangles) {
    auto F = convert_triangles(triangles);
    return ::igl::is_vertex_manifold(F);
}

TrianglesAndIndexMap make_manifold(const std::span<const glm::uvec3> triangles) {
    auto F = convert_triangles(triangles);
    Eigen::MatrixX3i SF;
    Eigen::VectorXi SVI;
    ::igl::split_nonmanifold(F, SF, SVI);
    return {
        convert_triangles(SF),
        to_stl_vector<decltype(SVI), uint32_t>(SVI)
    };
}

} // namespace mesh

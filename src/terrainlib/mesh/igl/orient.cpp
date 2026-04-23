#include <vector>
#include <span>

#include <glm/common.hpp>
#include <igl/bfs_orient.h>

#include "mesh/igl/convert.h"

namespace mesh {

namespace detail {
void orient_triangles_core(
    const std::span<const glm::uvec3> triangles_in,
    const std::span<glm::uvec3> triangles_out) {
    auto F = convert_triangles(triangles_in);
    Eigen::MatrixX3i FF;
    Eigen::VectorXi C;
    igl::bfs_orient(F, FF, C);
    to_stl_glm_span(FF, triangles_out);
}
}

void orient_triangles_inplace(const std::span<glm::uvec3> triangles) {
    detail::orient_triangles_core(triangles, triangles);
}

std::vector<glm::uvec3> orient_triangles(const std::span<glm::uvec3> triangles) {
    std::vector<glm::uvec3> reoriented(triangles.size());
    detail::orient_triangles_core(triangles, reoriented);
    return reoriented;
}

} // namespace mesh

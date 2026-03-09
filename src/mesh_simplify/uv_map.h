#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <opencv2/core/mat.hpp>
#include <CGAL/Surface_mesh_parameterization/Error_code.h>
#include <CGAL/Unique_hash_map.h>
#include <tl/expected.hpp>

typedef cv::Mat Texture;

namespace uv_map {

enum class Algorithm {
    TutteBarycentricMapping,
    DiscreteAuthalic,
    DiscreteConformalMap,
    FloaterMeanValueCoordinates
};

enum class Border {
    Circle,
    Square
};

class UvParameterizationError {
public:
    UvParameterizationError() = default;
    constexpr UvParameterizationError(int code)
        : code(code) {}

    operator int() const {
        return this->code;
    }

    std::string description() const {
        return CGAL::Surface_mesh_parameterization::get_error_message(this->code);
    }

private:
    int code;
};

typedef CGAL::Unique_hash_map<VertexDescriptor, Point2> UvMap;
typedef boost::associative_property_map<UvMap> UvPropertyMap;

std::vector<glm::dvec2> decode_uv_map(const UvMap& uv_map, size_t number_of_vertices);
std::vector<glm::dvec2> decode_uv_map(const UvPropertyMap& uv_map, size_t number_of_vertices);

tl::expected<UvMap, UvParameterizationError> parameterize_mesh(
    SurfaceMesh &mesh,
    Algorithm algorithm,
    Border border);

tl::expected<UvMap, UvParameterizationError> parameterize_mesh(
    const SimpleMesh &mesh,
    Algorithm algorithm,
    Border border);

Texture merge_textures(
    const std::span<const SimpleMesh> original_meshes,
    const SimpleMesh& merged_mesh,
    const merge::VertexMapping &mapping,
    const UvMap& uv_map,
    const glm::uvec2 merged_texture_size);

}

#endif

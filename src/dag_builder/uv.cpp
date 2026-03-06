#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/Surface_mesh_parameterization/Error_code.h>
#include <CGAL/Surface_mesh_parameterization/Barycentric_mapping_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Circular_border_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Discrete_authalic_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Discrete_conformal_map_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Mean_value_coordinates_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Square_border_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/parameterize.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#include <CGAL/Unique_hash_map.h>
#pragma GCC diagnostic pop

#include "mesh/cgal.h"
#include "mesh/convert.h"
#include "mesh/validate.h"
#include "uv.h"

namespace uv {

std::string UnwrapError::description() const {
    return CGAL::Surface_mesh_parameterization::get_error_message(this->code);
}

namespace {
using CircleBorderParameterizer = CGAL::Surface_mesh_parameterization::Circular_border_uniform_parameterizer_3<cgal::Mesh>;
using SquareBorderParameterizer = CGAL::Surface_mesh_parameterization::Square_border_uniform_parameterizer_3<cgal::Mesh>;
template <typename Border>
using TutteBarycentricMappingParameterizer = CGAL::Surface_mesh_parameterization::Barycentric_mapping_parameterizer_3<cgal::Mesh, Border>;
template <typename Border>
using DiscreteAuthalicParameterizer = CGAL::Surface_mesh_parameterization::Discrete_authalic_parameterizer_3<cgal::Mesh, Border>;
template <typename Border>
using DiscreteConformalMapParameterizer = CGAL::Surface_mesh_parameterization::Discrete_conformal_map_parameterizer_3<cgal::Mesh, Border>;
template <typename Border>
using FloaterMeanValueCoordinatesParameterizer = CGAL::Surface_mesh_parameterization::Mean_value_coordinates_parameterizer_3<cgal::Mesh, Border>;
using TutteBarycentricMappingParameterizerCircularBorder = TutteBarycentricMappingParameterizer<CircleBorderParameterizer>;
using TutteBarycentricMappingParameterizerSquareBorder = TutteBarycentricMappingParameterizer<SquareBorderParameterizer>;
using DiscreteAuthalicParameterizerCircularBorder = DiscreteAuthalicParameterizer<CircleBorderParameterizer>;
using DiscreteAuthalicParameterizerSquareBorder = DiscreteAuthalicParameterizer<SquareBorderParameterizer>;
using DiscreteConformalMapParameterizerCircularBorder = DiscreteConformalMapParameterizer<CircleBorderParameterizer>;
using DiscreteConformalMapParameterizerSquareBorder = DiscreteConformalMapParameterizer<SquareBorderParameterizer>;
using FloaterMeanValueCoordinatesParameterizerCircularBorder = FloaterMeanValueCoordinatesParameterizer<CircleBorderParameterizer>;
using FloaterMeanValueCoordinatesParameterizerSquareBorder = FloaterMeanValueCoordinatesParameterizer<SquareBorderParameterizer>;

using CgalUvMap = CGAL::Unique_hash_map<cgal::VertexDescriptor, cgal::Point2>;
using CgalUvPropMap = boost::associative_property_map<CgalUvMap>;

tl::expected<CgalUvMap, UnwrapError> parameterize_mesh(cgal::Mesh &mesh, Algorithm algorithm, Border border) {

    const cgal::HalfedgeDescriptor bhd = CGAL::Polygon_mesh_processing::longest_border(mesh).first;

    CgalUvMap uv_uhm;
    CgalUvPropMap uv_map(uv_uhm);

    CGAL::Surface_mesh_parameterization::Error_code result;
    if (border != Border::Circle && border != Border::Square) {
        throw std::invalid_argument("illegal border specifier");
    }

    if (algorithm == Algorithm::TutteBarycentricMapping) {
        if (border == Border::Circle) {
            result = CGAL::Surface_mesh_parameterization::parameterize(mesh, TutteBarycentricMappingParameterizerCircularBorder(), bhd, uv_map);
        } else if (border == Border::Square) {
            result = CGAL::Surface_mesh_parameterization::parameterize(mesh, TutteBarycentricMappingParameterizerSquareBorder(), bhd, uv_map);
        }
    } else if (algorithm == Algorithm::DiscreteAuthalic) {
        if (border == Border::Circle) {
            result = CGAL::Surface_mesh_parameterization::parameterize(mesh, DiscreteAuthalicParameterizerCircularBorder(), bhd, uv_map);
        } else if (border == Border::Square) {
            result = CGAL::Surface_mesh_parameterization::parameterize(mesh, DiscreteAuthalicParameterizerSquareBorder(), bhd, uv_map);
        }
    } else if (algorithm == Algorithm::DiscreteConformalMap) {
        if (border == Border::Circle) {
            result = CGAL::Surface_mesh_parameterization::parameterize(mesh, DiscreteConformalMapParameterizerCircularBorder(), bhd, uv_map);
        } else if (border == Border::Square) {
            result = CGAL::Surface_mesh_parameterization::parameterize(mesh, DiscreteConformalMapParameterizerSquareBorder(), bhd, uv_map);
        }
    } else if (algorithm == Algorithm::FloaterMeanValueCoordinates) {
        if (border == Border::Circle) {
            result = CGAL::Surface_mesh_parameterization::parameterize(mesh, FloaterMeanValueCoordinatesParameterizerCircularBorder(), bhd, uv_map);
        } else if (border == Border::Square) {
            result = CGAL::Surface_mesh_parameterization::parameterize(mesh, FloaterMeanValueCoordinatesParameterizerSquareBorder(), bhd, uv_map);
        }
    } else {
        throw std::invalid_argument("illegal algorithm specifier");
    }

    if (result != CGAL::Surface_mesh_parameterization::OK) {
        return tl::unexpected(UnwrapError(result));
    }

    for (size_t i = 0; i < CGAL::num_vertices(mesh); i++) {
        cgal::Point2 &uv = uv_map[CGAL::SM_Vertex_index(i)];
        uv = convert::to_cgal_point<cgal::Kernel>(glm::clamp(convert::to_glm_point(uv), glm::dvec2(0), glm::dvec2(1)));
    }

    return uv_uhm;
}


std::vector<glm::dvec2> decode_uv_map(const CgalUvMap &map, size_t number_of_vertices) {
    std::vector<glm::dvec2> uvs;
    uvs.reserve(number_of_vertices);
    for (size_t i = 0; i < number_of_vertices; i++) {
        const cgal::Point2 &uv = map[CGAL::SM_Vertex_index(i)];
        uvs.push_back(convert::to_glm_point(uv));
    }
    return uvs;
}
}

tl::expected<Map, UnwrapError> unwrap(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec3> positions,
    Algorithm algorithm,
    Border border) {
    const mesh::View mesh(triangles, positions);
    mesh::validate_manifold(mesh);
    cgal::Mesh cgal_mesh = convert::to_cgal_mesh(mesh);
    auto result = parameterize_mesh(cgal_mesh, algorithm, border);
    if (!result) {
        return tl::unexpected(result.error());
    }
    const CgalUvMap cgal_uv_map = result.value();
    const Map uv_map = decode_uv_map(cgal_uv_map, mesh.vertex_count());
    return uv_map;
}
}

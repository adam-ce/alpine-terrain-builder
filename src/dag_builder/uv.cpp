#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/Surface_mesh_parameterization/ARAP_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Barycentric_mapping_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Circular_border_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Discrete_authalic_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Discrete_conformal_map_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Error_code.h>
#include <CGAL/Surface_mesh_parameterization/LSCM_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Mean_value_coordinates_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Square_border_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Two_vertices_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/parameterize.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#include <CGAL/Unique_hash_map.h>
#pragma GCC diagnostic pop

#include "mesh/cgal.h"
#include "mesh/convert.h"
#include "mesh/validate.h"
#include "uv.h"
#include "mesh/compute_topology.h"

namespace uv {

std::string UnwrapError::description() const {
    return CGAL::Surface_mesh_parameterization::get_error_message(this->code);
}

namespace {
using CircleBorderParameterizer = CGAL::Surface_mesh_parameterization::Circular_border_uniform_parameterizer_3<cgal::Mesh>;
using SquareBorderParameterizer = CGAL::Surface_mesh_parameterization::Square_border_uniform_parameterizer_3<cgal::Mesh>;
using TwoVerticesBorderParameterizer = CGAL::Surface_mesh_parameterization::Two_vertices_parameterizer_3<cgal::Mesh>;

template <typename Border>
using TutteBarycentricMappingParameterizer =
    CGAL::Surface_mesh_parameterization::Barycentric_mapping_parameterizer_3<cgal::Mesh, Border>;
template <typename Border>
using DiscreteAuthalicParameterizer =
    CGAL::Surface_mesh_parameterization::Discrete_authalic_parameterizer_3<cgal::Mesh, Border>;
template <typename Border>
using DiscreteConformalMapParameterizer =
    CGAL::Surface_mesh_parameterization::Discrete_conformal_map_parameterizer_3<cgal::Mesh, Border>;
template <typename Border>
using FloaterMeanValueCoordinatesParameterizer =
    CGAL::Surface_mesh_parameterization::Mean_value_coordinates_parameterizer_3<cgal::Mesh, Border>;
template <typename Border>
using LSCMParameterizer =
    CGAL::Surface_mesh_parameterization::LSCM_parameterizer_3<cgal::Mesh, Border>;
template <typename Border>
using ARAPParameterizer =
    CGAL::Surface_mesh_parameterization::ARAP_parameterizer_3<cgal::Mesh, Border>;

using TutteBarycentricMappingParameterizerCircularBorder = TutteBarycentricMappingParameterizer<CircleBorderParameterizer>;
using TutteBarycentricMappingParameterizerSquareBorder = TutteBarycentricMappingParameterizer<SquareBorderParameterizer>;
using DiscreteAuthalicParameterizerCircularBorder = DiscreteAuthalicParameterizer<CircleBorderParameterizer>;
using DiscreteAuthalicParameterizerSquareBorder = DiscreteAuthalicParameterizer<SquareBorderParameterizer>;
using DiscreteConformalMapParameterizerCircularBorder = DiscreteConformalMapParameterizer<CircleBorderParameterizer>;
using DiscreteConformalMapParameterizerSquareBorder = DiscreteConformalMapParameterizer<SquareBorderParameterizer>;
using FloaterMeanValueCoordinatesParameterizerCircularBorder = FloaterMeanValueCoordinatesParameterizer<CircleBorderParameterizer>;
using FloaterMeanValueCoordinatesParameterizerSquareBorder = FloaterMeanValueCoordinatesParameterizer<SquareBorderParameterizer>;

using LSCMParameterizerFreeBorder = LSCMParameterizer<TwoVerticesBorderParameterizer>;
using ARAPParameterizerFreeBorder = ARAPParameterizer<CircleBorderParameterizer>;

using CgalUvMap = CGAL::Unique_hash_map<cgal::VertexDescriptor, cgal::Point2>;
using CgalUvPropMap = boost::associative_property_map<CgalUvMap>;

inline cgal::Point2 max(const cgal::Point2 &a, const cgal::Point2 &b) {
    return cgal::Point2(std::max(a.x(), b.x()), std::max(a.y(), b.y()));
}

inline cgal::Point2 min(const cgal::Point2 &a, const cgal::Point2 &b) {
    return cgal::Point2(std::min(a.x(), b.x()), std::min(a.y(), b.y()));
}

inline void check_uv(const glm::uvec2 &uv) {
    DEBUG_ASSERT(uv.x >= 0.0 && uv.x <= 1.0);
    DEBUG_ASSERT(uv.y >= 0.0 && uv.y <= 1.0);
}

template <typename UvMap>
void normalize_uv_map(UvMap &map, size_t vertex_count) {
    using FT = cgal::Kernel::FT;
    using Vector_2 = cgal::Kernel::Vector_2;

    constexpr double inf = std::numeric_limits<double>::infinity();
    cgal::Point2 lo(inf, inf);
    cgal::Point2 hi(-inf, -inf);

    for (size_t i = 0; i < vertex_count; i++) {
        const auto v = cgal::VertexIndex(i);
        const auto &uv = get(map, v);
        lo = min(lo, uv);
        hi = max(hi, uv);
    }

    const Vector_2 extent = hi - lo;
    const FT longest_side = std::max(extent.x(), extent.y());
    const FT scale = 1 / longest_side;

    for (size_t i = 0; i < vertex_count; i++) {
        const auto v = cgal::VertexIndex(i);
        auto& uv = get(map, v);
        uv = CGAL::ORIGIN + (uv - lo) * scale;
        check_uv(convert::to_glm_point(uv));
    }
}

template <typename UvMap>
void clamp_uv_map(UvMap &map, size_t vertex_count) {
    using FT = cgal::Kernel::FT;

    for (size_t i = 0; i < vertex_count; i++) {
        const auto v = cgal::VertexIndex(i);
        const auto& uv = get(map, v);
        const cgal::Point2 clamped(
            std::clamp(uv.x(), FT(0), FT(1)),
            std::clamp(uv.y(), FT(0), FT(1)));
        put(map, v, clamped);
    }
}

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
    } else if (algorithm == Algorithm::LeastSquaresConformalMap) {
        // free border
        result = CGAL::Surface_mesh_parameterization::parameterize(mesh,
            LSCMParameterizerFreeBorder(), bhd, uv_map);
    } else if (algorithm == Algorithm::AsRigidAsPossible) {
        // free border
        result = CGAL::Surface_mesh_parameterization::parameterize(mesh,
            ARAPParameterizerFreeBorder(1000.0), bhd, uv_map);
    } else {
        throw std::invalid_argument("illegal algorithm specifier");
    }

    if (result != CGAL::Surface_mesh_parameterization::OK) {
        return tl::unexpected(UnwrapError(result));
    }

    const auto vertex_count = CGAL::num_vertices(mesh);
    if (is_free_border(algorithm)) {
        rotate_uv_map_to_best_fit_box(uv_map, vertex_count);
        normalize_uv_map(uv_map, vertex_count);
    } else {
        clamp_uv_map(uv_map, vertex_count);
    }

    return uv_uhm;
}

template <typename UvMap>
std::vector<glm::dvec2> decode_uv_map(const UvMap &map, size_t vertex_count) {
    std::vector<glm::dvec2> uvs;
    uvs.reserve(vertex_count);
    for (size_t i = 0; i < vertex_count; i++) {
        const cgal::Point2 &cgal_uv = map[CGAL::SM_Vertex_index(i)];
        const glm::dvec2 uv = convert::to_glm_point(cgal_uv);
        check_uv(uv);
        uvs.push_back(uv);
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
    DEBUG_ASSERT(mesh::compute_topology(mesh).is_disk(true));
    
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

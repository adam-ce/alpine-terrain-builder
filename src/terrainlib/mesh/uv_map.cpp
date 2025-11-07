#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/Surface_mesh_parameterization/Circular_border_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Square_border_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Barycentric_mapping_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Discrete_authalic_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Discrete_conformal_map_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/Mean_value_coordinates_parameterizer_3.h>
#include <CGAL/Surface_mesh_parameterization/parameterize.h>
#include <CGAL/Surface_mesh_parameterization/Error_code.h>
#include <CGAL/Unique_hash_map.h>

#include "mesh/convert.h"
#include "merge.h"
#include "uv_map.h"

using namespace uv_map;

std::string UvParameterizationError::description() const {
    return CGAL::Surface_mesh_parameterization::get_error_message(this->code);
}

tl::expected<void, UvParameterizationError> uv_map::parameterize_mesh(cgal::Mesh &mesh, Algorithm algorithm, Border border) {
    using CircleBorderParameterizer = CGAL::Surface_mesh_parameterization::Circular_border_uniform_parameterizer_3<cgal::Mesh>;
    using SquareBorderParameterizer = CGAL::Surface_mesh_parameterization::Square_border_uniform_parameterizer_3<cgal::Mesh>;

    using TutteBarycentricMappingParameterizerCircularBorder = CGAL::Surface_mesh_parameterization::Barycentric_mapping_parameterizer_3<cgal::Mesh, CircleBorderParameterizer>;
    using TutteBarycentricMappingParameterizerSquareBorder = CGAL::Surface_mesh_parameterization::Barycentric_mapping_parameterizer_3<cgal::Mesh, SquareBorderParameterizer>;
    using DiscreteAuthalicParameterizerCircularBorder = CGAL::Surface_mesh_parameterization::Discrete_authalic_parameterizer_3<cgal::Mesh, CircleBorderParameterizer>;
    using DiscreteAuthalicParameterizerSquareBorder = CGAL::Surface_mesh_parameterization::Discrete_authalic_parameterizer_3<cgal::Mesh, SquareBorderParameterizer>;
    using DiscreteConformalMapParameterizerCircularBorder = CGAL::Surface_mesh_parameterization::Discrete_conformal_map_parameterizer_3<cgal::Mesh, CircleBorderParameterizer>;
    using DiscreteConformalMapParameterizerSquareBorder = CGAL::Surface_mesh_parameterization::Discrete_conformal_map_parameterizer_3<cgal::Mesh, SquareBorderParameterizer>;
    using FloaterMeanValueCoordinatesParameterizerCircularBorder = CGAL::Surface_mesh_parameterization::Mean_value_coordinates_parameterizer_3<cgal::Mesh, CircleBorderParameterizer>;
    using FloaterMeanValueCoordinatesParameterizerSquareBorder = CGAL::Surface_mesh_parameterization::Mean_value_coordinates_parameterizer_3<cgal::Mesh, SquareBorderParameterizer>;

    const cgal::HalfedgeDescriptor bhd = CGAL::Polygon_mesh_processing::longest_border(mesh).first;

    auto uv_map = mesh.add_property_map<cgal::VertexDescriptor, cgal::Point2>("v:uv").first;

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
        return tl::unexpected(UvParameterizationError(result));
    }

    for (cgal::VertexDescriptor v : CGAL::vertices(mesh)) {
        auto &uv = uv_map[v];
        glm::dvec2 g = convert::to_glm_point(uv);
        g = glm::clamp(g, glm::dvec2(0), glm::dvec2(1));
        uv = convert::to_cgal_point<cgal::Kernel>(g);
    }

    return {};
}

namespace {
inline std::vector<glm::dvec2> decode_uv_map(const auto &uv_map, size_t number_of_vertices) {
    std::vector<glm::dvec2> uvs;
    uvs.reserve(number_of_vertices);
    for (size_t i = 0; i < number_of_vertices; i++) {
        const cgal::Point2 &uv = uv_map[CGAL::SM_Vertex_index(i)];
        uvs.push_back(convert::to_glm_point(uv));
    }
    return uvs;
}
}

tl::expected<std::vector<glm::dvec2>, UvParameterizationError> uv_map::parameterize_mesh(const SimpleMesh &mesh, Algorithm algorithm, Border border) {
    cgal::Mesh cgal_mesh = convert::to_cgal_mesh(mesh);
    const auto result = parameterize_mesh(cgal_mesh, algorithm, border);
    if (!result) {
        return tl::unexpected(result.error());
    }
    auto uv_map_opt = cgal_mesh.property_map<cgal::VertexDescriptor, cgal::Point2>("v:uv");
    ASSERT(uv_map_opt.has_value());
    const auto &uv_map = uv_map_opt.value();
    return decode_uv_map(uv_map, mesh.vertex_count());
}

template <typename T>
inline cv::Rect_<T> clamp_rect_to_mat_bounds(const cv::Rect_<T> &rect, const cv::Mat &mat) {
    const T x = std::max(rect.x, 0);
    const T y = std::max(rect.y, 0);
    const T width = std::max(std::min(rect.width, mat.cols - x), 0);
    const T height = std::max(std::min(rect.height, mat.rows - y), 0);

    return cv::Rect(x, y, width, height);
}

void warp_triangle(
    const cv::Mat &source_image,
    cv::Mat &target_image,
    std::array<cv::Point2f, 3> source_triangle,
    std::array<cv::Point2f, 3> target_triangle
    // TODO: const uint32_t padding = 1
) {
    // Find bounding rectangle for each triangle
    const cv::Rect source_rect = clamp_rect_to_mat_bounds(cv::boundingRect(source_triangle), source_image);
    const cv::Rect target_rect = clamp_rect_to_mat_bounds(cv::boundingRect(target_triangle), target_image);
    if (source_rect.width == 0 || source_rect.height == 0
     || target_rect.width == 0 || target_rect.height == 0) {
        return;
    }


    // Relativize triangles to bounds
    std::array<cv::Point2f, 3> source_triangle_cropped;
    std::array<cv::Point2f, 3> target_triangle_cropped;
    for (size_t i = 0; i < 3; i++) {
        source_triangle_cropped[i] = cv::Point2f(source_triangle[i].x - source_rect.x, source_triangle[i].y - source_rect.y);
        target_triangle_cropped[i] = cv::Point2f(target_triangle[i].x - target_rect.x, target_triangle[i].y - target_rect.y);
    }

    // Convert points to int triangles as fillConvexPoly needs a vector of Point2i and not Point2f
    std::array<cv::Point2i, 3> target_triangle_cropped_int;
    for (size_t i = 0; i < 3; i++) {
        target_triangle_cropped_int[i]= cv::Point2i((int)(target_triangle[i].x - target_rect.x), (int)(target_triangle[i].y - target_rect.y));
    }

    // Read source region from source image
    cv::Mat source_image_cropped;
    source_image(source_rect).copyTo(source_image_cropped);
    source_image_cropped.convertTo(source_image_cropped, CV_32FC3);

    // Given a pair of triangles, find the affine transform.
    const cv::Mat warp_tranform = cv::getAffineTransform(source_triangle_cropped, target_triangle_cropped);

    // Apply the affine transform just found to the source image
    cv::Mat target_image_cropped = cv::Mat::zeros(target_rect.height, target_rect.width, CV_32FC3);
    cv::warpAffine(source_image_cropped, target_image_cropped, warp_tranform, target_image_cropped.size(), cv::INTER_LINEAR, cv::BORDER_REFLECT_101);

    // Get mask by filling triangle
    cv::Mat mask = cv::Mat::zeros(target_rect.height, target_rect.width, CV_32FC3);
    cv::fillConvexPoly(mask, target_triangle_cropped_int, cv::Scalar(1.0, 1.0, 1.0), 16, 0);

    // Copy triangular region of the rectangular patch to the output image
    cv::multiply(target_image_cropped, mask, target_image_cropped, 1, CV_32FC3);
    cv::multiply(target_image(target_rect), cv::Scalar(1.0, 1.0, 1.0) - mask, target_image(target_rect), 1, CV_32FC3);
    target_image(target_rect) = target_image(target_rect) + target_image_cropped;
}

// TODO: reproject triangles
/*
Texture uv_map::merge_textures(
    const std::span<const std::reference_wrapper<const SimpleMesh>> original_meshes,
    const SimpleMesh &merged_mesh,
    const mesh::merging::VertexMapping &mapping,
    const UvMap &uv_map,
    const glm::uvec2 merged_texture_size) {
    for (const SimpleMesh& mesh : original_meshes) {
        DEBUG_ASSERT(mesh.has_texture());
    }

    cv::Mat merged_atlas = cv::Mat::zeros(merged_texture_size.y, merged_texture_size.x, CV_32FC3);

    for (const glm::uvec3 &mapped_triangle : merged_mesh.triangles) {
        std::array<cv::Point2f, 3> mapped_uv_triangle;
        for (size_t i = 0; i < static_cast<size_t>(mapped_triangle.length()); i++) {
            glm::dvec2 uv = convert::to_glm_point(uv_map[cgal::VertexIndex(mapped_triangle[i])]);
            // TODO: Fix for black borders
            uv = (uv - 0.5) * 1.01 + 0.5;
            mapped_uv_triangle[i] = cv::Point2f(uv.x * merged_texture_size.x, uv.y * merged_texture_size.y);
        }

        const mesh::merging::TriangleInMesh source_mesh_and_triangle = mapping.find_source_triangle(mapped_triangle);
        const size_t source_mesh_index = source_mesh_and_triangle.mesh_index;
        const SimpleMesh &source_mesh = original_meshes[source_mesh_index];
        const glm::uvec3 source_triangle = source_mesh_and_triangle.triangle;

        std::array<cv::Point2f, 3> source_uv_triangle;
        for (size_t i = 0; i < static_cast<size_t>(source_triangle.length()); i++) {
            const glm::dvec2 uv = source_mesh.uvs[source_triangle[i]];
            const cv::Size source_texture_size = source_mesh.texture->size();
            source_uv_triangle[i] = cv::Point2f(uv.x * source_texture_size.width, uv.y * source_texture_size.height);
        }

        warp_triangle(source_mesh.texture.value(), merged_atlas, source_uv_triangle, mapped_uv_triangle);
    }

    return merged_atlas;
}
*/

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <radix/geometry.h>

#include "fixed_point.h"
#include "mesh/WindingOrder.h"
#include "Vector2D.h"

struct ReprojectionOptions {
    uint32_t sample_scale = 2;
    int interpolation = cv::INTER_LINEAR;
};

struct ReprojectionTriangle {
    uint32_t source_image_index = 0;
    std::array<glm::dvec2, 3> source_uvs;
    std::array<glm::dvec2, 3> target_uvs;
};

namespace {

using FixedCoord = fixed::Q<16>;
using FixedPoint = fp::Vec2<FixedCoord>;
using FixedScalar = fp::Scalar<FixedCoord>;
using FixedTriangle = std::array<FixedPoint, 3>;

struct PreparedTriangle {
    const cv::Mat *source_image = nullptr;
    FixedTriangle source_pixels;
    FixedTriangle target_samples;
    FixedScalar target_area2{};
};

glm::dvec2 clamp_uv(const glm::dvec2 &uv) {
    return glm::clamp(uv, glm::dvec2(0.0), glm::dvec2(1.0));
}

FixedPoint sample_cell_center(const glm::uvec2 sample) {
    return FixedPoint::from_real(glm::dvec2(sample) + 0.5);
}

bool is_left_of_or_on_edge(const FixedPoint edge_start, const FixedPoint edge_end, const FixedPoint sample) {
    return fp::orient(edge_start, edge_end, sample) >= FixedScalar::zero();
}

radix::geometry::Aabb2ui clamp_bounds(
    const radix::geometry::Aabb2ui &bounds,
    const radix::geometry::Aabb2ui &clamp_to) {
    return radix::geometry::Aabb2ui(
        glm::clamp(bounds.min, clamp_to.min, clamp_to.max),
        glm::clamp(bounds.max, clamp_to.min, clamp_to.max));
}

radix::geometry::Aabb2ui get_pixel_bounds(
    const FixedTriangle &triangle,
    const glm::uvec2 grid_size) {
    const FixedPoint lo = fp::min(triangle[0], triangle[1], triangle[2]);
    const FixedPoint hi = fp::max(triangle[0], triangle[1], triangle[2]);

    const radix::geometry::Aabb2ui pixel_bounds(
        glm::uvec2(lo.real()),
        glm::uvec2(hi.real()));

    const radix::geometry::Aabb2ui grid_bounds(
        glm::uvec2(0, 0),
        grid_size - glm::uvec2(1, 1));

    return clamp_bounds(pixel_bounds, grid_bounds);
}

void rasterize_triangle(
    const FixedTriangle &triangle,
    const int32_t triangle_id,
    const glm::uvec2 grid_size,
    Vector2D<int32_t> &triangle_index_map) {
    const radix::geometry::Aabb2ui bounds = get_pixel_bounds(triangle, grid_size);
    for (uint32_t sample_y = bounds.min.y; sample_y <= bounds.max.y; sample_y++) {
        for (uint32_t sample_x = bounds.min.x; sample_x <= bounds.max.x; sample_x++) {
            const FixedPoint sample = sample_cell_center({sample_x, sample_y});

            if (!is_left_of_or_on_edge(triangle[0], triangle[1], sample) ||
                !is_left_of_or_on_edge(triangle[1], triangle[2], sample) ||
                !is_left_of_or_on_edge(triangle[2], triangle[0], sample)) {
                continue;
            }

            triangle_index_map(sample_y, sample_x) = triangle_id;
        }
    }
}

// Source coordinates are in source-pixel space.
// Target coordinates are in supersample-grid space.
std::vector<PreparedTriangle> prepare_triangles(
    const std::span<const cv::Mat> source_images,
    const std::span<const ReprojectionTriangle> triangles,
    const glm::uvec2 output_size,
    const uint32_t sample_scale) {
    std::vector<PreparedTriangle> prepared;
    prepared.reserve(triangles.size());

    const glm::dvec2 target_sample_size = glm::dvec2(output_size) * static_cast<double>(sample_scale);

    for (const ReprojectionTriangle &triangle : triangles) {
        const cv::Mat& source_image = source_images[triangle.source_image_index];
        const glm::dvec2 source_size(source_image.cols, source_image.rows);

        PreparedTriangle prepared_triangle;
        prepared_triangle.source_image = &source_image;

        for (uint8_t k = 0; k < 3; k++) {
            const glm::dvec2 source_uv = clamp_uv(triangle.source_uvs[k]);
            const glm::dvec2 target_uv = clamp_uv(triangle.target_uvs[k]);

            prepared_triangle.source_pixels[k] = FixedPoint::from_real(source_uv * source_size);
            prepared_triangle.target_samples[k] = FixedPoint::from_real(target_uv * target_sample_size);
        }

        prepared_triangle.target_area2 = fp::orient(
            prepared_triangle.target_samples[0],
            prepared_triangle.target_samples[1],
            prepared_triangle.target_samples[2]);

        // Ignore empty triangles.
        if (prepared_triangle.target_area2 == FixedScalar::zero()) {
            continue;
        }

        // Fix incorrect winding order.
        if (prepared_triangle.target_area2 < FixedScalar::zero()) {
            prepared_triangle.target_samples = swap_orientation(prepared_triangle.target_samples);
            prepared_triangle.source_pixels = swap_orientation(prepared_triangle.source_pixels);
            prepared_triangle.target_area2 = -prepared_triangle.target_area2;
        }

        // Keep only front-facing triangles.
        prepared.push_back(prepared_triangle);
    }

    return prepared;
}

Vector2D<int32_t> build_triangle_index_map(const std::span<const PreparedTriangle> triangles, const glm::uvec2 grid_size) {
    Vector2D<int32_t> triangle_index_map(grid_size.y, grid_size.x, -1);

    for (uint32_t triangle_index = 0; triangle_index < triangles.size(); triangle_index++) {
        rasterize_triangle(
            triangles[triangle_index].target_samples,
            triangle_index,
            grid_size,
            triangle_index_map);
    }

    return triangle_index_map;
}

glm::dvec2 target_to_source(const PreparedTriangle &triangle, const FixedPoint target_sample) {
    // Barycentric weights from oriented sub-triangle areas.
    const FixedScalar w0 = (triangle.target_samples[1] - target_sample).cross(triangle.target_samples[2] - target_sample) / triangle.target_area2;
    const FixedScalar w1 = (triangle.target_samples[2] - target_sample).cross(triangle.target_samples[0] - target_sample) / triangle.target_area2;
    // Compute w2 by subtraction to guarantee w0 + w1 + w2 == FixedScalar::one() exactly.
    const FixedScalar w2 = FixedScalar::one() - w0 - w1;

    const FixedPoint source = triangle.source_pixels[0] * w0 +
                              triangle.source_pixels[1] * w1 +
                              triangle.source_pixels[2] * w2;

    return source.real();
}

// Clamp-to-edge: out-of-bounds coordinates reuse the nearest border pixel.
cv::Vec3f source_texel(const cv::Mat &image, const glm::ivec2 pos) {
    const glm::ivec2 clamped = glm::clamp(pos, glm::ivec2(0), glm::ivec2(image.cols - 1, image.rows - 1));
    return cv::Vec3f(image.at<cv::Vec3b>(clamped.y, clamped.x)) * (1.0f / 255.0f);
}

cv::Vec3f sample_source_linear(
    const cv::Mat &image,
    const glm::dvec2 &point) {
    const glm::ivec2 p0 = glm::ivec2(glm::floor(point));
    const glm::dvec2 t = point - glm::dvec2(p0);

    const cv::Vec3f c00 = source_texel(image, p0);
    const cv::Vec3f c10 = source_texel(image, p0 + glm::ivec2(1, 0));
    const cv::Vec3f c01 = source_texel(image, p0 + glm::ivec2(0, 1));
    const cv::Vec3f c11 = source_texel(image, p0 + glm::ivec2(1, 1));

    return c00 * static_cast<float>((1.0 - t.x) * (1.0 - t.y)) +
           c10 * static_cast<float>(t.x * (1.0 - t.y)) +
           c01 * static_cast<float>((1.0 - t.x) * t.y) +
           c11 * static_cast<float>(t.x * t.y);
}

cv::Vec3f sample_source_nearest(
    const cv::Mat &image,
    const glm::dvec2 &point) {
    return source_texel(image, glm::ivec2(glm::round(point)));
}

cv::Vec3f sample_source(
    const cv::Mat &image,
    const glm::dvec2 &point,
    const int interpolation) {
    switch (interpolation) {
    case cv::INTER_NEAREST:
        return sample_source_nearest(image, point);

    case cv::INTER_LINEAR:
        return sample_source_linear(image, point);

    default:
        throw std::invalid_argument("TextureReprojector supports INTER_NEAREST and INTER_LINEAR");
    }
}

} // namespace

class TextureReprojector {
public:
    TextureReprojector(
        const glm::uvec2 output_size,
        const int output_type = CV_8UC3,
        ReprojectionOptions options = {})
        : _output_size(output_size),
          _output_type(output_type),
          _options(options) {
    }

    cv::Mat render(const std::span<const cv::Mat> source_images, const std::span<const ReprojectionTriangle> triangles) {
        const std::vector<PreparedTriangle> prepared_triangles =
            prepare_triangles(source_images, triangles, this->_output_size, this->_options.sample_scale);

        const glm::uvec2 grid_size = this->_output_size * this->_options.sample_scale;
        const Vector2D<int32_t> triangle_index_map =
            build_triangle_index_map(prepared_triangles, grid_size);

        cv::Mat output(this->_output_size.y, this->_output_size.x, CV_32FC3, cv::Scalar(0, 0, 0));

        for (uint32_t y = 0; y < this->_output_size.y; y++) {
            for (uint32_t x = 0; x < this->_output_size.x; x++) {
                cv::Vec3f color(0, 0, 0);
                uint32_t sample_count = 0;

                for (uint32_t sy = 0; sy < this->_options.sample_scale; sy++) {
                    for (uint32_t sx = 0; sx < this->_options.sample_scale; sx++) {
                        const glm::uvec2 sample(
                            x * this->_options.sample_scale + sx,
                            y * this->_options.sample_scale + sy);

                        const int32_t triangle_id = triangle_index_map(sample.y, sample.x);
                        if (triangle_id < 0) {
                            continue;
                        }

                        const PreparedTriangle &triangle = prepared_triangles[triangle_id];
                        const FixedPoint target_sample = sample_cell_center(sample);
                        const glm::dvec2 source_sample = target_to_source(triangle, target_sample);

                        // Offset by 0.5 to convert from pixel-center to OpenCV texel-index space.
                        color += sample_source(
                            *triangle.source_image,
                            source_sample - glm::dvec2(0.5),
                            this->_options.interpolation);

                        sample_count++;
                    }
                }

                // Dividing by sample_count (not sample_scale^2) correctly averages partially covered pixels.
                if (sample_count > 0) {
                    output.at<cv::Vec3f>(y, x) = color / static_cast<float>(sample_count);
                }
            }
        }

        cv::Mat converted;
        output.convertTo(converted, this->_output_type);
        return converted;
    }

private:
    glm::uvec2 _output_size;
    int _output_type;
    ReprojectionOptions _options;
};

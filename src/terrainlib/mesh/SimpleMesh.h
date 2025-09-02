#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>
#include <radix/geometry.h>
#include <zpp_bits.h>

template <glm::length_t dimensions = 3, typename T = double>
class SimpleMesh_ {
public:
    static constexpr glm::length_t n_dims = dimensions;
    using Component = T;
    using Triangle = glm::uvec3;
    using Edge = glm::uvec2;
    using Position = glm::vec<dimensions, T>;
    using Uv = glm::vec<2, T>;
    using Texture = cv::Mat;
    using serialize = zpp::bits::members<4>;

    SimpleMesh_(std::vector<Triangle> triangles, std::vector<Position> positions) 
        : SimpleMesh_(triangles, positions, {}) {}
    SimpleMesh_(std::vector<Triangle> triangles, std::vector<Position> positions, std::vector<Uv> uvs) 
        : SimpleMesh_(triangles, positions, uvs, std::nullopt) {}
    SimpleMesh_(std::vector<Triangle> triangles, std::vector<Position> positions, std::vector<Uv> uvs, Texture texture)
        : SimpleMesh_(triangles, positions, uvs, std::optional<Texture>(std::move(texture))) {}
    SimpleMesh_(std::vector<Triangle> triangles, std::vector<Position> positions, std::vector<Uv> uvs, std::optional<Texture> texture)
        : triangles(triangles), positions(positions), uvs(uvs), texture(std::move(texture)) {}
    SimpleMesh_() = default;
    SimpleMesh_(SimpleMesh_ &&) = default;
    SimpleMesh_ &operator=(SimpleMesh_ &&) = default;
    SimpleMesh_(const SimpleMesh_ &) = default;
    SimpleMesh_ &operator=(const SimpleMesh_ &) = default;

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
    bool is_empty() const {
        return this->vertex_count() == 0 && this->face_count() == 0;
    }

    bool has_uvs() const {
        return this->uvs.size() > 0;
    }
    bool has_texture() const {
        return this->texture.has_value();
    }
};

using SimpleMesh3d = SimpleMesh_<3, double>;
using SimpleMesh2d = SimpleMesh_<2, double>;

using SimpleMesh = SimpleMesh3d;

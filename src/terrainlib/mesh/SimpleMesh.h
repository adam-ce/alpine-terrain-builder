#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>
#include <radix/geometry.h>
#include <zpp_bits.h>

namespace mesh {

template <glm::length_t dimensions = 3, typename T = double>
class Simple_ {
public:
    static constexpr glm::length_t n_dims = dimensions;
    using Component = T;
    using Triangle = glm::uvec3;
    using Edge = glm::uvec2;
    using Position = glm::vec<dimensions, T>;
    using Uv = glm::vec<2, T>;
    using Texture = cv::Mat;

    Simple_(std::vector<Triangle> triangles, std::vector<Position> positions)
        : Simple_(std::move(triangles), std::move(positions), {}) {}
    Simple_(std::vector<Triangle> triangles, std::vector<Position> positions, std::vector<Uv> uvs)
        : Simple_(std::move(triangles), std::move(positions), std::move(uvs), std::nullopt) {}
    Simple_(std::vector<Triangle> triangles, std::vector<Position> positions, std::vector<Uv> uvs, Texture texture)
        : Simple_(std::move(triangles), std::move(positions), std::move(uvs), std::optional<Texture>(std::move(texture))) {}
    Simple_(std::vector<Triangle> triangles, std::vector<Position> positions, std::vector<Uv> uvs, std::optional<Texture> texture)
        : triangles(std::move(triangles)), positions(std::move(positions)), uvs(std::move(uvs)), texture(std::move(texture)) {}
    Simple_() = default;
    Simple_(Simple_ &&) = default;
    Simple_ &operator=(Simple_ &&) = default;
    Simple_(const Simple_ &) = default;
    Simple_ &operator=(const Simple_ &) = default;

    std::vector<Triangle> triangles;
    std::vector<Position> positions;
    std::vector<Uv> uvs;
    std::optional<Texture> texture;

    void clear() {
        this->triangles.clear();
        this->positions.clear();
        this->uvs.clear();
        this->texture = std::nullopt;
    }

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

using Simple3d = Simple_<3, double>;
using Simple2d = Simple_<2, double>;
using Simple3f = Simple_<3, float>;
using Simple2f = Simple_<3, float>;

using Simple = Simple3d;
using Shared = std::shared_ptr<Simple>;

}

template <glm::length_t n_dims = 3, typename T = double>
using SimpleMesh_ = mesh::Simple_<n_dims, T>;

using SimpleMesh3d = mesh::Simple3d;
using SimpleMesh2d = mesh::Simple2d;

using SimpleMesh = mesh::Simple3d;

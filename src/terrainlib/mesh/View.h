#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "mesh/SimpleMesh.h"

namespace mesh {

template <glm::length_t dimensions = 3, typename T = double>
class View_ {
public:
    static constexpr glm::length_t n_dims = dimensions;
    using Component = T;
    using Triangle = glm::uvec3;
    using Edge = glm::uvec2;
    using Position = glm::vec<dimensions, T>;
    using Uv = glm::vec<2, T>;
    using Texture = cv::Mat;

    View_(std::span<const Triangle> triangles, std::span<const Position> positions)
        : View_(triangles, positions, {}, std::nullopt) {}
    View_(std::span<const Triangle> triangles, std::span<const Position> positions, std::span<const Uv> uvs)
        : View_(triangles, positions, uvs, std::nullopt) {}
    View_(std::span<const Triangle> triangles, std::span<const Position> positions, std::span<const Uv> uvs, Texture texture)
        : View_(triangles, positions, uvs, std::optional<Texture>(std::move(texture))) {}
    View_(std::span<const Triangle> triangles, std::span<const Position> positions, std::span<const Uv> uvs, std::optional<Texture> texture)
        : triangles(triangles), positions(positions), uvs(uvs), texture(std::move(texture)) {}
    View_() = default;
    View_(View_ &&) = default;
    View_ &operator=(View_ &&) = default;
    View_(const View_ &) = default;
    View_ &operator=(const View_ &) = default;
    View_(const mesh::Simple_<dimensions, T> &m)
        : View_(m.triangles, m.positions, m.uvs, m.texture) {}

    std::span<const Triangle> triangles;
    std::span<const Position> positions;
    std::span<const Uv> uvs;
    std::optional<Texture> texture;

    void clear() {
        this->triangles = {};
        this->positions = {};
        this->uvs = {};
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

using View3d = View_<3, double>;
using View2d = View_<2, double>;
using View3f = View_<3, float>;
using View2f = View_<2, float>;

using View = View3d;

} // namespace mesh

template <glm::length_t n_dims = 3, typename T = double>
using MeshView_ = mesh::View_<n_dims, T>;

using MeshView3d = mesh::View3d;
using MeshView2d = mesh::View2d;

using MeshView = mesh::View3d;
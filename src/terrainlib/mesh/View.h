#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "mesh/SimpleMesh.h"

namespace mesh {

template <bool IsMut, glm::length_t dimensions = 3, typename T = double>
class View__ {
private:
    template <typename U>
    using maybe_const_t = std::conditional_t<IsMut, U, const U>;

public:
    static constexpr glm::length_t n_dims = dimensions;
    static constexpr bool is_mut = IsMut;
    static constexpr bool is_const = !is_mut;

    using Triangle = glm::uvec3;
    using Edge = glm::uvec2;
    using Position = glm::vec<dimensions, T>;
    using Uv = glm::vec<2, T>;
    using Texture = cv::Mat;

    using TriangleSpan = std::span<maybe_const_t<Triangle>>;
    using PositionSpan = std::span<maybe_const_t<Position>>;
    using UvSpan = std::span<maybe_const_t<Uv>>;

    View__(TriangleSpan triangles, PositionSpan positions)
        : View__(triangles, positions, {}, std::nullopt) {}

    View__(TriangleSpan triangles, PositionSpan positions, UvSpan uvs)
        : View__(triangles, positions, uvs, std::nullopt) {}

    View__(TriangleSpan triangles, PositionSpan positions, UvSpan uvs, Texture texture)
        : View__(triangles, positions, uvs, std::optional<Texture>(std::move(texture))) {}

    View__(TriangleSpan triangles, PositionSpan positions, UvSpan uvs, std::optional<Texture> texture)
        : triangles(triangles), positions(positions), uvs(uvs), texture(std::move(texture)) {}

    View__() = default;
    View__(View__ &&) = default;
    View__ &operator=(View__ &&) = default;
    View__(const View__ &) = default;
    View__ &operator=(const View__ &) = default;

    View__(mesh::Simple_<dimensions, T> &m)
        requires(IsMut)
        : View__(m.triangles, m.positions, m.uvs, m.texture) {}

    View__(const mesh::Simple_<dimensions, T> &m)
        requires(!IsMut)
        : View__(m.triangles, m.positions, m.uvs, m.texture) {}

    TriangleSpan triangles;
    PositionSpan positions;
    UvSpan uvs;
    std::optional<Texture> texture;

    void clear() requires(IsMut) {
        triangles = {};
        positions = {};
        uvs = {};
        texture = std::nullopt;
    }

    size_t vertex_count() const {
        return this->positions.size();
    }
    size_t face_count() const {
        return this->triangles.size();
    }
    size_t uv_count() const {
        return this->uvs.size();
    }

    bool is_empty() const {
        return this->vertex_count() == 0 && this->face_count() == 0;
    }
    bool has_uvs() const {
        return this->uv_count() > 0;
    }
    bool has_texture() const {
        return texture.has_value();
    }

    operator View__<false, dimensions, T>() const {
        return View__<false, dimensions, T>{triangles, positions, uvs, texture};
    }
};

template <glm::length_t n_dims = 3, typename T = double>
using View_ = View__<false, n_dims, T>;
template <glm::length_t n_dims = 3, typename T = double>
using MutView_ = View__<true, n_dims, T>;

using View3d = View_<3, double>;
using View2d = View_<2, double>;
using View3f = View_<3, float>;
using View2f = View_<2, float>;

using MutView3d = MutView_<3, double>;
using MutView2d = MutView_<2, double>;
using MutView3f = MutView_<3, float>;
using MutView2f = MutView_<2, float>;

using View = View3d;
using MutView = MutView3d;

} // namespace mesh

template <bool IsConst, glm::length_t n_dims = 3, typename T = double>
using MeshView__ = mesh::View__<IsConst, n_dims, T>;

template <glm::length_t n_dims = 3, typename T = double>
using MeshView_ = mesh::View_<n_dims, T>;
template <glm::length_t n_dims = 3, typename T = double>
using MeshMutView_ = mesh::View_<n_dims, T>;

using MeshView3d = mesh::View_<3, double>;
using MeshView2d = mesh::View_<2, double>;
using MeshView3f = mesh::View_<3, float>;
using MeshView2f = mesh::View_<2, float>;

using MeshMutView3d = mesh::MutView_<3, double>;
using MeshMutView2d = mesh::MutView_<2, double>;
using MeshMutView3f = mesh::MutView_<3, float>;
using MeshMutView2f = mesh::MutView_<2, float>;

using MeshView = mesh::View3d;
using MeshMutView = mesh::MutView3d;

#include <algorithm>
#include <array>
#include <compare>
#include <ranges>
#include <vector>

#include <glm/common.hpp>

#include "mesh/SimpleMesh.h"

template <glm::length_t n_dims, typename T>
TriangleSoup_<n_dims, T> to_triangle_soup(const mesh::Simple_<n_dims, T> &mesh) {
    TriangleSoup_<n_dims, T> soup;
    soup.reserve(mesh.triangles.size());
    for (const glm::uvec3 &triangle : mesh.triangles) {
        soup.push_back({mesh.positions[triangle[0]],
                        mesh.positions[triangle[1]],
                        mesh.positions[triangle[2]]});
    }
    return soup;
}

namespace detail {
template <glm::length_t n_dims, typename T>
[[nodiscard]] constexpr std::strong_ordering vertex_compare(
    const glm::vec<n_dims, T> &a,
    const glm::vec<n_dims, T> &b) noexcept {
    for (uint8_t k = 0; k < n_dims; k++) {
        if (a[k] < b[k]) {
            return std::strong_ordering::less;
        }
        if (b[k] < a[k]) {
            return std::strong_ordering::greater;
        }
    }
    return std::strong_ordering::equal;
}

template <glm::length_t n_dims, typename T>
[[nodiscard]] constexpr std::strong_ordering triangle_compare(
    const std::array<glm::vec<n_dims, T>, 3> &a,
    const std::array<glm::vec<n_dims, T>, 3> &b) noexcept {
    for (uint8_t k = 0; k < 3; k++) {
        const auto cmp = vertex_compare(a[k], b[k]);
        if (cmp != std::strong_ordering::equal) {
            return cmp;
        }
    }
    return std::strong_ordering::equal;
}

template <glm::length_t n_dims, typename T>
[[nodiscard]] constexpr std::strong_ordering triangle_soup_compare(
    const TriangleSoup_<n_dims, T> &a,
    const TriangleSoup_<n_dims, T> &b) noexcept {
    const auto size_cmp = a.size() <=> b.size();
    if (size_cmp != std::strong_ordering::equal) {
        return size_cmp;
    }

    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        const auto cmp = triangle_compare(a[i], b[i]);
        if (cmp != std::strong_ordering::equal) {
            return cmp;
        }
    }

    return std::strong_ordering::equal;
}

template <glm::length_t n_dims, typename T>
void normalize_triangle(std::array<glm::vec<n_dims, T>, 3> &triangle) {
    auto min_it = std::ranges::min_element(triangle, [](const auto &a, const auto &b) {
        return vertex_compare(a, b) == std::strong_ordering::less;
    });
    std::rotate(triangle.begin(), min_it, triangle.end());
}
} // namespace detail

template <glm::length_t n_dims, typename T>
void sort_triangle_soup(TriangleSoup_<n_dims, T> &soup) {
    // Rotate to put the smallest vertex first
    for (auto &triangle : soup) {
        detail::normalize_triangle(triangle);
    }

    // Sort the list of triangles
    std::sort(soup.begin(), soup.end(), [](const auto &a, const auto &b) {
        return detail::triangle_compare(a, b) == std::strong_ordering::less;
    });
}

template <glm::length_t n_dims, typename T>
TriangleSoup_<n_dims, T> to_sorted_triangle_soup(const mesh::Simple_<n_dims, T> &mesh) {
    TriangleSoup_<n_dims, T> soup = to_triangle_soup(mesh);
    sort_triangle_soup(soup);
    return soup;
}

template <glm::length_t n_dims, typename T>
auto to_sorted_triangle_soups(const std::vector<mesh::Simple_<n_dims, T>> &meshes) {
    std::vector<TriangleSoup_<n_dims, T>> soups;
    soups.reserve(meshes.size());
    for (const auto &mesh : meshes) {
        soups.push_back(to_sorted_triangle_soup(mesh));
    }
    std::sort(soups.begin(), soups.end(), [](const auto &a, const auto &b) {
        return detail::triangle_soup_compare(a, b) == std::strong_ordering::less;
    });
    return soups;
}

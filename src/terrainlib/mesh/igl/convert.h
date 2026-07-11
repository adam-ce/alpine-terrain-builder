#pragma once

#include <concepts>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

#include <Eigen/Core>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <libassert/assert.hpp>

namespace detail {
template <std::size_t Extent>
inline constexpr int span_extent_to_eigen_v =
    (Extent == std::dynamic_extent) ? Eigen::Dynamic
                                    : static_cast<int>(Extent);

template <int E>
inline constexpr std::size_t eigen_extent_to_span_v =
    (E == Eigen::Dynamic) ? std::dynamic_extent
                          : static_cast<std::size_t>(E);
}

// span -> eigen
template <
    typename TIn,
    typename TOut = TIn,
    size_t SIn = std::dynamic_extent,
    int SOut = detail::span_extent_to_eigen_v<SIn>>
void to_eigen_vector(
    const std::span<const TIn, SIn> data,
    Eigen::Vector<TOut, SOut> &out) {
    static_assert(std::is_convertible_v<TIn, TOut>);
    static_assert(SIn == SOut || SOut == Eigen::Dynamic);

    if constexpr (SOut == Eigen::Dynamic) {
        out.resize(data.size());
    }

    for (size_t i = 0; i < out.rows(); i++) {
        out(i) = static_cast<TOut>(data[i]);
    }
}
template <
    typename TIn,
    size_t SIn = std::dynamic_extent,
    typename TOut = TIn,
    int SOut = detail::span_extent_to_eigen_v<SIn>>
Eigen::Vector<TOut, SOut> to_eigen_vector(const std::span<const TIn, SIn> data) {
    Eigen::Vector<TOut, SOut> out;
    to_eigen_vector<TIn, TOut, SIn, SOut>(data, out);
    return out;
}

// eigen -> span
template <
    typename Derived,
    typename TOut = typename Derived::Scalar>
void to_stl_span(
    const Eigen::MatrixBase<Derived> &data,
    std::span<TOut> out) {
    using TIn = typename Derived::Scalar;
    static_assert(std::is_convertible_v<TIn, TOut>);
    static_assert(Derived::ColsAtCompileTime == 1);

    ASSERT(out.size() == data.size());
    for (size_t i = 0; i < static_cast<size_t>(data.size()); i++) {
        out[i] = static_cast<TOut>(data(i));
    }
}

// eigen -> vector
template <
    typename Derived,
    typename TOut = typename Derived::Scalar>
void to_stl_vector(
    const Eigen::MatrixBase<Derived> &data,
    std::vector<TOut> &out) {
    out.resize(data.size());
    to_stl_span(data, std::span(out));
}
template <
    typename Derived,
    typename TOut = typename Derived::Scalar>
std::vector<TOut> to_stl_vector(const Eigen::MatrixBase<Derived> &data) {
    std::vector<TOut> out;
    to_stl_vector(data, out);
    return out;
}

// glm -> eigen
template <
    typename TIn,
    glm::length_t SIn,
    typename TOut = TIn,
    int SOut = SIn,
    glm::qualifier Q = glm::defaultp>
void to_eigen_vector(
    const glm::vec<SIn, TIn, Q> &data,
    Eigen::Vector<TOut, SOut> &out) {
    const std::span<const TIn, SIn> span(glm::value_ptr(data), SIn);
    to_eigen_vector(span, out);
}
template <
    typename TIn,
    glm::length_t SIn,
    typename TOut = TIn,
    int SOut = SIn,
    glm::qualifier Q = glm::defaultp>
Eigen::Vector<TOut, SOut> to_eigen_vector(const glm::vec<SIn, TIn, Q> &data) {
    Eigen::Vector<TOut, SOut> out;
    to_eigen_vector<TIn, SIn, TOut, SOut, Q>(data, out);
    return out;
}

// eigen -> glm
template <
    typename Derived,
    typename TOut = typename Derived::Scalar,
    glm::qualifier Q = glm::defaultp>
void to_glm_vector(
    const Eigen::MatrixBase<Derived> &data,
    glm::vec<Derived::RowsAtCompileTime, TOut, Q> &out) {
    using TIn = typename Derived::Scalar;
    static_assert(std::is_convertible_v<TIn, TOut>);
    static_assert(Derived::ColsAtCompileTime == 1);
    static_assert(Derived::RowsAtCompileTime != Eigen::Dynamic);

    for (size_t i = 0; i < data.size(); i++) {
        out[i] = static_cast<TOut>(data(i));
    }
}
template <
    typename Derived,
    typename TOut = typename Derived::Scalar,
    glm::qualifier Q = glm::defaultp>
glm::vec<Derived::RowsAtCompileTime, TOut, Q> to_glm_vector(
    const Eigen::MatrixBase<Derived> &data) {
    glm::vec<Derived::RowsAtCompileTime, TOut, Q> out;
    to_glm_vector<Derived, TOut, Q>(data, out);
    return out;
}

// span<glm> -> eigen
template <
    typename TIn,
    size_t RIn,
    glm::length_t CIn,
    typename TOut = TIn,
    int ROut = detail::span_extent_to_eigen_v<RIn>,
    int COut = CIn,
    glm::qualifier Q = glm::defaultp>
void to_eigen_matrix(
    const std::span<const glm::vec<CIn, TIn, Q>, RIn> data,
    Eigen::Matrix<TOut, ROut, COut> &out) {
    static_assert(std::is_convertible_v<TIn, TOut>);
    static_assert(RIn == ROut || ROut == Eigen::Dynamic);
    static_assert(CIn == COut || COut == Eigen::Dynamic);

    if constexpr (ROut == Eigen::Dynamic || COut == Eigen::Dynamic) {
        out.resize(data.size(), CIn);
    }

    for (size_t i = 0; i < data.size(); i++) {
        for (size_t j = 0; j < CIn; j++) {
            out(i, j) = static_cast<TOut>(data[i][j]);
        }
    }
}
template <
    typename TIn,
    size_t RIn,
    glm::length_t CIn,
    typename TOut = TIn,
    int ROut = detail::span_extent_to_eigen_v<RIn>,
    int COut = CIn,
    glm::qualifier Q = glm::defaultp>
Eigen::Matrix<TOut, ROut, COut> to_eigen_matrix(const std::span<const glm::vec<CIn, TIn, Q>, RIn> data) {
    Eigen::Matrix<TOut, ROut, COut> out;
    to_eigen_matrix<TIn, RIn, CIn, TOut, ROut, COut, Q>(data, out);
    return out;
}

// vector<glm> -> eigen
template <
    typename TIn,
    glm::length_t CIn,
    typename TOut = TIn,
    int ROut = Eigen::Dynamic,
    int COut = CIn,
    glm::qualifier Q = glm::defaultp>
void to_eigen_matrix(
    const std::vector<glm::vec<CIn, TIn, Q>> &data,
    Eigen::Matrix<TOut, ROut, COut> &out) {
    to_eigen_matrix<TIn, std::dynamic_extent, CIn, TOut, ROut, COut, Q>(std::span<const glm::vec<CIn, TIn, Q>>(data), out);
}
template <
    typename TIn,
    glm::length_t CIn,
    typename TOut = TIn,
    int ROut = Eigen::Dynamic,
    int COut = CIn,
    glm::qualifier Q = glm::defaultp>
Eigen::Matrix<TOut, ROut, COut> to_eigen_matrix(const std::vector<glm::vec<CIn, TIn, Q>>& data) {
    return to_eigen_matrix<TIn, std::dynamic_extent, CIn, TOut, ROut, COut, Q>(std::span<const glm::vec<CIn, TIn, Q>>(data));
}

// eigen -> span<glm>
template <
    typename Derived,
    typename TOut = typename Derived::Scalar,
    glm::qualifier Q = glm::defaultp>
void to_stl_glm_span(
    const Eigen::MatrixBase<Derived> &data,
    std::span<glm::vec<Derived::ColsAtCompileTime, TOut, Q>> out) {
    using TIn = typename Derived::Scalar;
    static_assert(std::is_convertible_v<TIn, TOut>);
    static_assert(Derived::ColsAtCompileTime != Eigen::Dynamic);

    ASSERT(out.size() == data.rows());

    for (size_t i = 0; i < static_cast<size_t>(data.rows()); i++) {
        for (size_t j = 0; j < static_cast<size_t>(data.cols()); j++) {
            out[i][j] = static_cast<TOut>(data(i, j));
        }
    }
}

// eigen -> vector<glm>
template <
    typename Derived,
    typename TOut = typename Derived::Scalar,
    glm::qualifier Q = glm::defaultp>
void to_stl_glm_vector(
    const Eigen::MatrixBase<Derived> &data,
    std::vector<glm::vec<Derived::ColsAtCompileTime, TOut, Q>> &out) {
    out.resize(data.rows());
    to_stl_glm_span(data, std::span(out));
}
template <
    typename Derived,
    typename TOut = typename Derived::Scalar,
    glm::qualifier Q = glm::defaultp>
std::vector<glm::vec<Derived::ColsAtCompileTime, TOut, Q>> to_stl_glm_vector(const Eigen::MatrixBase<Derived> &data) {
    std::vector<glm::vec<Derived::ColsAtCompileTime, TOut, Q>> out;
    to_stl_glm_vector<Derived, TOut, Q>(data, out);
    return out;
}

[[nodiscard]] inline Eigen::MatrixX3d convert_vertices(const std::span<const glm::dvec3> positions) {
    return to_eigen_matrix(positions);
}
[[nodiscard]] inline std::vector<glm::dvec3> convert_vertices(const Eigen::Ref<const Eigen::MatrixX3d> &V) {
    return to_stl_glm_vector(V);
}
[[nodiscard]] inline Eigen::MatrixX3i convert_triangles(const std::span<const glm::uvec3> triangles) {
    Eigen::MatrixX3i out;
    to_eigen_matrix(triangles, out);
    return out;
}
[[nodiscard]] inline std::vector<glm::uvec3> convert_triangles(const Eigen::Ref<const Eigen::MatrixX3i> &F) {
    DEBUG_ASSERT((F.array() >= 0).all());
    std::vector<glm::uvec3> out;
    to_stl_glm_vector(F, out);
    return out;
}

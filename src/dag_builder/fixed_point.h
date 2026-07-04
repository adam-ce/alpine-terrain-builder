#pragma once

#include "fixed.h"

#include <algorithm>
#include <cassert>
#include <compare>
#include <glm/glm.hpp>
#include <glm/vector_relational.hpp>
#include <optional>

namespace fp {

template <class Q>
struct Scalar {
    using repr_t = typename Q::repr_t;
    repr_t v = 0;

    static constexpr Scalar from_int(repr_t x) noexcept { return {x * Q::scale}; }
    static Scalar from_real(double x) { return {Q::from_real(x).value}; }
    static constexpr Scalar one() noexcept { return {Q::scale}; }
    static constexpr Scalar zero() noexcept { return {0}; }

    constexpr double real() const noexcept { return Q{v}.real(); }

    constexpr Scalar operator+(Scalar rhs) const noexcept { return {v + rhs.v}; }
    constexpr Scalar operator-(Scalar rhs) const noexcept { return {v - rhs.v}; }
    constexpr Scalar operator-() const noexcept { return {-v}; }
    constexpr Scalar operator*(Scalar rhs) const noexcept { return {Q::mul(v, rhs.v)}; }
    constexpr Scalar operator/(Scalar rhs) const noexcept { return {Q::div(v, rhs.v)}; }

    constexpr Scalar& operator+=(Scalar rhs) noexcept { v += rhs.v; return *this; }
    constexpr Scalar& operator-=(Scalar rhs) noexcept { v -= rhs.v; return *this; }

    constexpr auto operator<=>(const Scalar&) const noexcept = default;
};

template <class Q>
struct Vec2 {
    using repr_t = glm::vec<2, typename Q::repr_t>;
    using S = Scalar<Q>;

    repr_t raw{};

    static Vec2 from_real(double x, double y) {
        return {{Q::from_real(x).value, Q::from_real(y).value}};
    }
    static Vec2 from_real(glm::dvec2 p) { return from_real(p.x, p.y); }

    glm::dvec2 real() const { return {Q{raw.x}.real(), Q{raw.y}.real()}; }

    // Addition and subtraction are raw (same scale) -- delegate to glm.
    constexpr Vec2 operator+(Vec2 rhs) const noexcept { return {raw + rhs.raw}; }
    constexpr Vec2 operator-(Vec2 rhs) const noexcept { return {raw - rhs.raw}; }
    constexpr Vec2 operator-() const noexcept { return {-raw}; }

    constexpr Vec2& operator+=(Vec2 rhs) noexcept { raw += rhs.raw; return *this; }

    // Scaling rescales each component through Q::mul.
    constexpr Vec2 operator*(S s) const noexcept {
        return {{Q::mul(raw.x, s.v), Q::mul(raw.y, s.v)}};
    }

    // dot and cross use fixed-point multiply so the result is in the same scale.
    constexpr S dot(Vec2 rhs) const noexcept {
        return {Q::mul(raw.x, rhs.raw.x) + Q::mul(raw.y, rhs.raw.y)};
    }
    constexpr S cross(Vec2 rhs) const noexcept {
        return {Q::mul(raw.x, rhs.raw.y) - Q::mul(raw.y, rhs.raw.x)};
    }
    constexpr S length2() const noexcept { return dot(*this); }

    constexpr bool operator==(Vec2 rhs) const noexcept { return raw == rhs.raw; }
    constexpr bool operator!=(Vec2 rhs) const noexcept { return raw != rhs.raw; }
};

// Signed area test. Positive means p is left of directed edge a -> b.
template <class Q>
constexpr Scalar<Q> orient(Vec2<Q> a, Vec2<Q> b, Vec2<Q> p) noexcept {
    return (b - a).cross(p - a);
}

template <class Q>
constexpr bool collinear(Vec2<Q> a, Vec2<Q> b, Vec2<Q> p) noexcept {
    return orient(a, b, p) == Scalar<Q>::zero();
}

template <class Q>
constexpr bool left_of(Vec2<Q> a, Vec2<Q> b, Vec2<Q> p) noexcept {
    return orient(a, b, p) > Scalar<Q>::zero();
}

template <class Q>
constexpr bool right_of(Vec2<Q> a, Vec2<Q> b, Vec2<Q> p) noexcept {
    return orient(a, b, p) < Scalar<Q>::zero();
}

template <class Q>
constexpr Scalar<Q> distance2(Vec2<Q> a, Vec2<Q> b) noexcept {
    return (b - a).length2();
}

template <class Q>
bool inside_box(Vec2<Q> lo, Vec2<Q> hi, Vec2<Q> p) noexcept {
    return glm::all(glm::lessThanEqual(lo.raw, p.raw)) &&
           glm::all(glm::lessThanEqual(p.raw, hi.raw));
}

template <class Q>
Vec2<Q> min(Vec2<Q> a, Vec2<Q> b) noexcept {
    return {glm::min(a.raw, b.raw)};
}

template <class Q, class... Rest>
Vec2<Q> min(Vec2<Q> a, Vec2<Q> b, Rest... rest) noexcept {
    return min(min(a, b), rest...);
}

template <class Q>
Vec2<Q> max(Vec2<Q> a, Vec2<Q> b) noexcept {
    return {glm::max(a.raw, b.raw)};
}

template <class Q, class... Rest>
Vec2<Q> max(Vec2<Q> a, Vec2<Q> b, Rest... rest) noexcept {
    return max(max(a, b), rest...);
}

template <class Q>
bool on_segment(Vec2<Q> a, Vec2<Q> b, Vec2<Q> p) noexcept {
    return collinear(a, b, p) && inside_box(min(a, b), max(a, b), p);
}

// Interpolates from a to b using fixed-point parameter t in [0, Scalar::one()].
template <class Q>
constexpr Vec2<Q> lerp(Vec2<Q> a, Vec2<Q> b, Scalar<Q> t) noexcept {
    return a + (b - a) * t;
}

// Returns the fixed-point projection parameter of p onto the infinite line a -> b.
template <class Q>
constexpr Scalar<Q> project_t(Vec2<Q> a, Vec2<Q> b, Vec2<Q> p) noexcept {
    const Vec2<Q> ab = b - a;
    const Scalar<Q> denom = ab.length2();
    assert(denom.v != 0);
    return (p - a).dot(ab) / denom;
}

template <class Q>
constexpr Vec2<Q> project(Vec2<Q> a, Vec2<Q> b, Vec2<Q> p) noexcept {
    return lerp(a, b, project_t(a, b, p));
}

template <class Q>
constexpr Vec2<Q> closest_on_segment(Vec2<Q> a, Vec2<Q> b, Vec2<Q> p) noexcept {
    Scalar<Q> t = project_t(a, b, p);
    t = std::max(t, Scalar<Q>::zero());
    t = std::min(t, Scalar<Q>::one());
    return lerp(a, b, t);
}

template <class Q>
bool segments_intersect(Vec2<Q> a, Vec2<Q> b, Vec2<Q> c, Vec2<Q> d) noexcept {
    const auto ab_c = orient(a, b, c);
    const auto ab_d = orient(a, b, d);
    const auto cd_a = orient(c, d, a);
    const auto cd_b = orient(c, d, b);

    const auto zero = Scalar<Q>::zero();
    const bool ab_straddles = (ab_c > zero && ab_d < zero) || (ab_c < zero && ab_d > zero);
    const bool cd_straddles = (cd_a > zero && cd_b < zero) || (cd_a < zero && cd_b > zero);

    if (ab_straddles && cd_straddles) {
        return true;
    }

    return (ab_c == zero && on_segment(a, b, c)) ||
           (ab_d == zero && on_segment(a, b, d)) ||
           (cd_a == zero && on_segment(c, d, a)) ||
           (cd_b == zero && on_segment(c, d, b));
}

template <class Q>
std::optional<Vec2<Q>> line_intersection(Vec2<Q> a, Vec2<Q> b, Vec2<Q> c, Vec2<Q> d) noexcept {
    const Vec2<Q> ab = b - a;
    const Vec2<Q> cd = d - c;
    const Scalar<Q> denom = ab.cross(cd);

    if (denom.v == 0) {
        return std::nullopt;
    }

    const Scalar<Q> t = (c - a).cross(cd) / denom;
    return a + ab * t;
}

template <class Q>
std::optional<Vec2<Q>> segment_intersection(Vec2<Q> a, Vec2<Q> b, Vec2<Q> c, Vec2<Q> d) noexcept {
    if (!segments_intersect(a, b, c, d)) {
        return std::nullopt;
    }

    if (collinear(a, b, c) && collinear(a, b, d)) {
        if (on_segment(a, b, c)) return c;
        if (on_segment(a, b, d)) return d;
        if (on_segment(c, d, a)) return a;
        if (on_segment(c, d, b)) return b;
        return std::nullopt;
    }

    return line_intersection(a, b, c, d);
}

} // namespace fp

#pragma once

#include "fixed.h"

#include <algorithm>
#include <compare>
#include <glm/glm.hpp>
#include <glm/vector_relational.hpp>
#include <libassert/assert.hpp>
#include <optional>

namespace fp {

template <typename Q>
struct Scalar {
    using repr_t = typename Q::repr_t;
    repr_t v = 0;

    static constexpr Scalar from_int(repr_t x) noexcept { return {x * Q::scale}; }
    static Scalar from_real(double x) { return {Q::from_real(x).value}; }
    static constexpr Scalar one() noexcept { return {Q::scale}; }
    static constexpr Scalar zero() noexcept { return {0}; }

    constexpr double real() const noexcept { return Q{this->v}.real(); }

    constexpr Scalar operator+(Scalar rhs) const noexcept { return {this->v + rhs.v}; }
    constexpr Scalar operator-(Scalar rhs) const noexcept { return {this->v - rhs.v}; }
    constexpr Scalar operator-() const noexcept { return {-this->v}; }
    constexpr Scalar operator*(Scalar rhs) const noexcept { return {Q::mul(this->v, rhs.v)}; }
    constexpr Scalar operator/(Scalar rhs) const noexcept { return {Q::div(this->v, rhs.v)}; }

    constexpr Scalar& operator+=(Scalar rhs) noexcept { this->v += rhs.v; return *this; }
    constexpr Scalar& operator-=(Scalar rhs) noexcept { this->v -= rhs.v; return *this; }

    constexpr auto operator<=>(const Scalar&) const noexcept = default;
};

template <typename Q>
struct Vec2 {
    using repr_t = glm::vec<2, typename Q::repr_t>;

    repr_t raw{};

    static Vec2 from_real(double x, double y) {
        return {{Q::from_real(x).value, Q::from_real(y).value}};
    }
    static Vec2 from_real(glm::dvec2 p) { return from_real(p.x, p.y); }

    glm::dvec2 real() const { return {Q{this->raw.x}.real(), Q{this->raw.y}.real()}; }

    // Addition and subtraction are raw (same scale) -- delegate to glm.
    constexpr Vec2 operator+(Vec2 rhs) const noexcept { return {this->raw + rhs.raw}; }
    constexpr Vec2 operator-(Vec2 rhs) const noexcept { return {this->raw - rhs.raw}; }
    constexpr Vec2 operator-() const noexcept { return {-this->raw}; }

    constexpr Vec2& operator+=(Vec2 rhs) noexcept { this->raw += rhs.raw; return *this; }

    // Scaling rescales each component through Q::mul.
    constexpr Vec2 operator*(Scalar<Q> s) const noexcept {
        return {{Q::mul(this->raw.x, s.v), Q::mul(this->raw.y, s.v)}};
    }

    // dot and cross use fixed-point multiply so the result is in the same scale.
    constexpr Scalar<Q> dot(Vec2 rhs) const noexcept {
        return {Q::mul(this->raw.x, rhs.raw.x) + Q::mul(this->raw.y, rhs.raw.y)};
    }
    constexpr Scalar<Q> cross(Vec2 rhs) const noexcept {
        return {Q::mul(this->raw.x, rhs.raw.y) - Q::mul(this->raw.y, rhs.raw.x)};
    }
    constexpr Scalar<Q> length2() const noexcept { return this->dot(*this); }

    constexpr bool operator==(const Vec2&) const noexcept = default;
};

// Signed area test. Positive means p is left of directed edge a -> b.
template <typename Q>
constexpr Scalar<Q> orient(Vec2<Q> a, Vec2<Q> b, Vec2<Q> p) noexcept {
    return (b - a).cross(p - a);
}

template <typename Q>
constexpr bool collinear(Vec2<Q> a, Vec2<Q> b, Vec2<Q> p) noexcept {
    return orient(a, b, p) == Scalar<Q>::zero();
}

template <typename Q>
constexpr bool left_of(Vec2<Q> a, Vec2<Q> b, Vec2<Q> p) noexcept {
    return orient(a, b, p) > Scalar<Q>::zero();
}

template <typename Q>
constexpr bool right_of(Vec2<Q> a, Vec2<Q> b, Vec2<Q> p) noexcept {
    return orient(a, b, p) < Scalar<Q>::zero();
}

template <typename Q>
constexpr Scalar<Q> distance2(Vec2<Q> a, Vec2<Q> b) noexcept {
    return (b - a).length2();
}

template <typename Q>
bool inside_box(Vec2<Q> lo, Vec2<Q> hi, Vec2<Q> p) noexcept {
    return glm::all(glm::lessThanEqual(lo.raw, p.raw)) &&
           glm::all(glm::lessThanEqual(p.raw, hi.raw));
}

template <typename Q>
Vec2<Q> min(Vec2<Q> a, Vec2<Q> b) noexcept {
    return {glm::min(a.raw, b.raw)};
}

template <typename Q, typename... Rest>
Vec2<Q> min(Vec2<Q> a, Vec2<Q> b, Rest... rest) noexcept {
    return min(min(a, b), rest...);
}

template <typename Q>
Vec2<Q> max(Vec2<Q> a, Vec2<Q> b) noexcept {
    return {glm::max(a.raw, b.raw)};
}

template <typename Q, typename... Rest>
Vec2<Q> max(Vec2<Q> a, Vec2<Q> b, Rest... rest) noexcept {
    return max(max(a, b), rest...);
}

template <typename Q>
bool on_segment(Vec2<Q> a, Vec2<Q> b, Vec2<Q> p) noexcept {
    return collinear(a, b, p) && inside_box(min(a, b), max(a, b), p);
}

// Interpolates from a to b using fixed-point parameter t in [0, Scalar::one()].
template <typename Q>
constexpr Vec2<Q> lerp(Vec2<Q> a, Vec2<Q> b, Scalar<Q> t) noexcept {
    return a + (b - a) * t;
}

// Returns the fixed-point projection parameter of p onto the infinite line a -> b.
template <typename Q>
constexpr Scalar<Q> project_t(Vec2<Q> a, Vec2<Q> b, Vec2<Q> p) noexcept {
    const Vec2<Q> ab = b - a;
    const Scalar<Q> denom = ab.length2();
    ASSERT(denom.v != 0);
    return (p - a).dot(ab) / denom;
}

template <typename Q>
constexpr Vec2<Q> project(Vec2<Q> a, Vec2<Q> b, Vec2<Q> p) noexcept {
    return lerp(a, b, project_t(a, b, p));
}

template <typename Q>
constexpr Vec2<Q> closest_on_segment(Vec2<Q> a, Vec2<Q> b, Vec2<Q> p) noexcept {
    Scalar<Q> t = project_t(a, b, p);
    t = std::max(t, Scalar<Q>::zero());
    t = std::min(t, Scalar<Q>::one());
    return lerp(a, b, t);
}

template <typename Q>
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

template <typename Q>
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

template <typename Q>
std::optional<Vec2<Q>> segment_intersection(Vec2<Q> a, Vec2<Q> b, Vec2<Q> c, Vec2<Q> d) noexcept {
    if (!segments_intersect(a, b, c, d)) {
        return std::nullopt;
    }

    if (collinear(a, b, c) && collinear(a, b, d)) {
        if (on_segment(a, b, c)) {
            return c;
        }
        if (on_segment(a, b, d)) {
            return d;
        }
        if (on_segment(c, d, a)) {
            return a;
        }
        if (on_segment(c, d, b)) {
            return b;
        }
        return std::nullopt;
    }

    return line_intersection(a, b, c, d);
}

} // namespace fp

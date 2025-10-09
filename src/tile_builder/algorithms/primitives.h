#include <glm/detail/qualifier.hpp>
#include <glm/glm.hpp>
#include <type_traits>

#ifndef ALGORITHMS_PRIMITIVES_H
#define ALGORITHMS_PRIMITIVES_H

namespace primitives {

// two times the area of the ccw triangle with vertices a, b, and c.
// negative, if the triangle is cw
template <typename T>
inline T triAreaX2(const glm::tvec2<T>& a, const glm::tvec2<T>& b, const glm::tvec2<T>& c)
{
    // doesn't work for unsigned types. if you need that, come up with something :)
    static_assert(std::is_signed_v<T>);
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

enum class Winding : char {
    CW = -1,
    Undefined = 0,
    CCW = 1
};

template <typename T>
inline Winding winding(const glm::tvec2<T>& a, const glm::tvec2<T>& b, const glm::tvec2<T>& c)
{
    constexpr bool is_integral = std::is_integral_v<T>;
    if constexpr (is_integral) {
        using Signed = typename std::conditional<is_integral, typename std::make_signed<T>::type, T>::type;
        auto v = (Signed(b.x) - Signed(a.x)) * (Signed(c.y) - Signed(a.y)) - (Signed(b.y) - Signed(a.y)) * (Signed(c.x) - Signed(a.x));
        if (v == 0)
            return Winding::Undefined;
        if (v < 0)
            return Winding::CW;
        return Winding::CCW;
    } else {
        auto v = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (std::abs(v) < 0.0000000000000001)
            return Winding::Undefined;
        if (v < 0)
            return Winding::CW;
        return Winding::CCW;
    }
}

template <bool include_border = false, typename T>
inline bool ccw(const glm::tvec2<T>& a, const glm::tvec2<T>& b, const glm::tvec2<T>& c)
{
    constexpr bool is_integral = std::is_integral_v<T>;
    if constexpr (is_integral) {
        using Signed = typename std::conditional<is_integral, typename std::make_signed<T>::type, T>::type;
        if constexpr (include_border)
            return (Signed(b.x) - Signed(a.x)) * (Signed(c.y) - Signed(a.y)) >= (Signed(b.y) - Signed(a.y)) * (Signed(c.x) - Signed(a.x));
        return (Signed(b.x) - Signed(a.x)) * (Signed(c.y) - Signed(a.y)) > (Signed(b.y) - Signed(a.y)) * (Signed(c.x) - Signed(a.x));
    }
    if constexpr (include_border)
        return (b.x - a.x) * (c.y - a.y) >= (b.y - a.y) * (c.x - a.x);
    return (b.x - a.x) * (c.y - a.y) > (b.y - a.y) * (c.x - a.x);
}

template <bool include_straight = false, typename T>
inline bool rightOf(const glm::tvec2<T>& x, const glm::tvec2<T>& org, const glm::tvec2<T>& dest)
{
    return ccw<include_straight>(x, dest, org);
}

template <bool include_straight = false, typename T>
inline bool leftOf(const glm::tvec2<T>& x, const glm::tvec2<T>& org, const glm::tvec2<T>& dest)
{
    return ccw<include_straight>(x, org, dest);
}

// https://www.scratchapixel.com/lessons/3d-basic-rendering/rasterization-practical-implementation/rasterization-stage
// the bottom edge of the raster is not included!
template <typename T>
inline bool inside(const glm::tvec2<T>& x, const glm::tvec2<T>& a, const glm::tvec2<T>& b, const glm::tvec2<T>& c)
{
    constexpr bool is_integral = std::is_integral_v<T>;
    using Signed = typename std::conditional<is_integral, typename std::make_signed<T>::type, T>::type;
    using sVec = glm::tvec2<Signed>;
    //  return leftOf<bool(include_border)>(x, a, b) && leftOf<bool(include_border)>(x, b, c) && leftOf<bool(include_border)>(x, c, a);
    const auto ccw = [](Winding w) { return w == Winding::CCW; };
    const auto undef = [](Winding w) { return w == Winding::Undefined; };
    const auto topleftedge = [](const sVec& edge) { return (edge.y == 0 && edge.x < 0) || edge.y < 0; };

    const auto w_ab = winding(x, a, b);
    const auto w_bc = winding(x, b, c);
    const auto w_ca = winding(x, c, a);
    const auto e_ab = sVec(b) - sVec(a);
    const auto e_bc = sVec(c) - sVec(b);
    const auto e_ca = sVec(a) - sVec(c);

    bool overlap = true;
    overlap &= ccw(w_ab) || (undef(w_ab) && topleftedge(e_ab));
    overlap &= ccw(w_bc) || (undef(w_bc) && topleftedge(e_bc));
    overlap &= ccw(w_ca) || (undef(w_ca) && topleftedge(e_ca));
    return overlap;
}
}

#endif

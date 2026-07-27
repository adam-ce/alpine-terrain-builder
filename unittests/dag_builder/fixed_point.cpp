#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "fixed.h"
#include "fixed_point.h"

TEST_CASE("round_div rounds half away from zero", "[dag_builder][fixed_point]") {
    using Q = fixed::Q<16>;

    // Positive half rounds up
    CHECK(Q::round_div(1, 2) == 1);
    CHECK(Q::round_div(3, 2) == 2);

    // Negative half rounds away from zero (down in absolute value)
    CHECK(Q::round_div(-1, 2) == -1);
    CHECK(Q::round_div(-3, 2) == -2);

    // Below half rounds down
    CHECK(Q::round_div(7, 3) == 2);
    CHECK(Q::round_div(-7, 3) == -2);

    // Above half rounds up
    CHECK(Q::round_div(7, 4) == 2);
    CHECK(Q::round_div(-7, 4) == -2);

    // Negative denominator normalizes correctly
    CHECK(Q::round_div(7, -4) == Q::round_div(-7, 4));
    CHECK(Q::round_div(-7, -4) == Q::round_div(7, 4));
}

TEST_CASE("fixed::Q mul performs fixed-point multiplication with rounding", "[dag_builder][fixed_point]") {
    using Q = fixed::Q<16>;

    // Small integer multiplication preserves scale
    const auto a_val = Q::from_integer(2).value;
    const auto b_val = Q::from_integer(3).value;
    const auto expected = Q::from_integer(6).value;
    CHECK(Q::mul(a_val, b_val) == expected);

    // Half-LSB case: multiplying fixed-point 0.5 (32768) by itself
    const auto half = Q::from_real(0.5).value;
    const auto quarter = Q::mul(half, half);
    CHECK(quarter == Q::from_real(0.25).value);

    // Very small multiplication rounds to zero
    CHECK(Q::mul(1, 1) == 0);

    // Inverse relationship with division for values at scale
    const auto a = Q::scale;
    const auto b = Q::scale * 3;
    const auto product = Q::mul(a, b);
    const auto divided = Q::div(product, a);
    CHECK(divided == b);
}

TEST_CASE("fixed::Q div performs fixed-point division with rounding", "[dag_builder][fixed_point]") {
    using Q = fixed::Q<16>;

    // Division is inverse of multiplication for exactly representable values
    const auto a_val = Q::from_integer(6).value;
    const auto b_val = Q::from_integer(2).value;
    const auto quotient = Q::div(a_val, b_val);
    CHECK(quotient == Q::from_integer(3).value);

    // Dividing by same value gives 1
    CHECK(Q::div(Q::scale * 7, Q::scale * 7) == Q::scale);

    // Half division
    const auto one = Q::scale;
    const auto two = Q::from_integer(2).value;
    const auto half = Q::div(one, two);
    CHECK(half == Q::from_real(0.5).value);
}

TEST_CASE("fp::orient detects triangle winding and collinearity", "[dag_builder][fixed_point]") {
    using Q = fixed::Q<16>;
    using FixedPoint = fp::Vec2<Q>;
    using FixedScalar = fp::Scalar<Q>;

    // CCW triple gives positive orient
    const auto a = FixedPoint::from_real(0.0, 0.0);
    const auto b = FixedPoint::from_real(1.0, 0.0);
    const auto p_ccw = FixedPoint::from_real(0.5, 0.5);
    const auto orient_ccw = fp::orient(a, b, p_ccw);
    CHECK(orient_ccw > FixedScalar::zero());

    // CW triple gives negative orient
    const auto p_cw = FixedPoint::from_real(0.5, -0.5);
    const auto orient_cw = fp::orient(a, b, p_cw);
    CHECK(orient_cw < FixedScalar::zero());

    // Collinear points give zero orient
    const auto p_collinear = FixedPoint::from_real(0.5, 0.0);
    const auto orient_collinear = fp::orient(a, b, p_collinear);
    CHECK(orient_collinear == FixedScalar::zero());

    // Collinear with integer coordinates
    const auto p1 = FixedPoint::from_real(0.0, 0.0);
    const auto p2 = FixedPoint::from_real(2.0, 0.0);
    const auto p3 = FixedPoint::from_real(1.0, 0.0);
    CHECK(fp::orient(p1, p2, p3) == FixedScalar::zero());

    // Half-coordinate collinearity
    const auto q1 = FixedPoint::from_real(0.0, 0.0);
    const auto q2 = FixedPoint::from_real(1.0, 0.0);
    const auto q3 = FixedPoint::from_real(0.5, 0.0);
    CHECK(fp::orient(q1, q2, q3) == FixedScalar::zero());
}

#include "../catch2_helpers.h"

#include <catch2/catch_approx.hpp>

#include "mask.h"

TEST_CASE("Mask simplification uses a fixed tenth of a metre") {
    OGRSpatialReference projected;
    REQUIRE(projected.importFromEPSG(31287) == OGRERR_NONE);

    const std::optional<double> projected_tolerance = mask::simplification_tolerance(projected);
    REQUIRE(projected_tolerance);
    CHECK(*projected_tolerance == Catch::Approx(0.1));

    OGRSpatialReference geographic;
    REQUIRE(geographic.SetWellKnownGeogCS("WGS84") == OGRERR_NONE);

    const std::optional<double> geographic_tolerance = mask::simplification_tolerance(geographic);
    REQUIRE(geographic_tolerance);
    CHECK(*geographic_tolerance == Catch::Approx(
        0.1 / geographic.GetSemiMajor() / geographic.GetAngularUnits()));
}

TEST_CASE("Mask simplification preserves polygon topology") {
    OGRLinearRing outer;
    outer.addPoint(0.0, 0.0);
    outer.addPoint(1.0, 0.01);
    outer.addPoint(2.0, 0.0);
    outer.addPoint(2.0, 2.0);
    outer.addPoint(0.0, 2.0);
    outer.addPoint(0.0, 0.0);

    OGRLinearRing hole;
    hole.addPoint(0.5, 0.5);
    hole.addPoint(1.0, 0.51);
    hole.addPoint(1.5, 0.5);
    hole.addPoint(1.5, 1.5);
    hole.addPoint(0.5, 1.5);
    hole.addPoint(0.5, 0.5);

    OGRPolygon polygon;
    REQUIRE(polygon.addRing(&outer) == OGRERR_NONE);
    REQUIRE(polygon.addRing(&hole) == OGRERR_NONE);
    REQUIRE(polygon.IsValid());

    const uint64_t original_point_count = mask::point_count(polygon);
    std::unique_ptr<OGRGeometry> simplified = mask::simplify_geometry(polygon, 0.1);

    REQUIRE(simplified);
    REQUIRE_FALSE(simplified->IsEmpty());
    REQUIRE(simplified->IsValid());
    CHECK(mask::point_count(*simplified) < original_point_count);

    const OGRPolygon *simplified_polygon = simplified->toPolygon();
    REQUIRE(simplified_polygon);
    CHECK(simplified_polygon->getNumInteriorRings() == 1);
}

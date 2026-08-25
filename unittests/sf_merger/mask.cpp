#include "../catch2_helpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>

#include "mask.h"
#include "earth.h"
#include "geometry/geometry.h"
#include "mesh/cgal.h"
#include "mesh/connectivity/manifold.h"
#include "mesh/convert.h"
#include "mesh/geometry.h"
#include "octree/Id.h"
#include "octree/Space.h"
#include "utils.h"

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

namespace {

Polygon2 make_square(const glm::dvec2 min, const glm::dvec2 max) {
    Polygon2 polygon;
    polygon.push_back(Point2(min.x, min.y));
    polygon.push_back(Point2(max.x, min.y));
    polygon.push_back(Point2(max.x, max.y));
    polygon.push_back(Point2(min.x, max.y));
    return polygon;
}

SpherePolygonMask make_sphere_polygon_mask(const bool point_touching) {
    MultipolygonWithHoles2 polygons;
    polygons.add_polygon(make_square(glm::dvec2(-0.04, -0.02), glm::dvec2(-0.02, 0.02)));
    const double second_min_x = point_touching ? -0.02 : 0.02;
    polygons.add_polygon(make_square(glm::dvec2(second_min_x, 0.02), glm::dvec2(0.04, 0.06)));
    return SpherePolygonMask{std::move(polygons), SphereProjector(glm::dvec3(100.0, 0.0, 0.0))};
}

void check_closed_volume(const SimpleMesh &mesh) {
    const cgal::Mesh cgal_mesh = convert::to_cgal_mesh(mesh);
    CHECK(mesh::is_manifold(mesh));
    CHECK(CGAL::is_closed(cgal_mesh));
    CHECK(CGAL::is_valid_polygon_mesh(cgal_mesh));
    CHECK_FALSE(CGAL::Polygon_mesh_processing::does_self_intersect(cgal_mesh));
    CHECK(CGAL::Polygon_mesh_processing::does_bound_a_volume(cgal_mesh));
}

SimpleMesh make_box(const glm::dvec3 min, const glm::dvec3 max) {
    return SimpleMesh(
        {
            {0, 2, 1}, {0, 3, 2},
            {4, 5, 6}, {4, 6, 7},
            {0, 1, 5}, {0, 5, 4},
            {1, 2, 6}, {1, 6, 5},
            {2, 3, 7}, {2, 7, 6},
            {3, 0, 4}, {3, 4, 7},
        },
        {
            {min.x, min.y, min.z},
            {max.x, min.y, min.z},
            {max.x, max.y, min.z},
            {min.x, max.y, min.z},
            {min.x, min.y, max.z},
            {max.x, min.y, max.z},
            {max.x, max.y, max.z},
            {min.x, max.y, max.z},
        });
}

SimpleMesh make_plane() {
    return SimpleMesh(
        {{0, 1, 2}, {0, 2, 3}},
        {
            {-3.0, -1.0, 0.0},
            {3.0, -1.0, 0.0},
            {3.0, 1.0, 0.0},
            {-3.0, 1.0, 0.0},
        });
}

double area(const SimpleMesh &mesh) {
    double result = 0.0;
    for (const glm::uvec3 triangle : mesh.triangles) {
        result += geometry::compute_triangle_area(
            mesh.positions[triangle.x],
            mesh.positions[triangle.y],
            mesh.positions[triangle.z]);
    }
    return result;
}

} // namespace

TEST_CASE("mask triangulation and extrusion preserve polygon components") {
    for (const bool point_touching : {false, true}) {
        CAPTURE(point_touching);

        const SphereMeshMask triangulated = mask::triangulate(make_sphere_polygon_mask(point_touching));
        REQUIRE(triangulated.components.size() == 2);
        CHECK_FALSE(triangulated.components[0].is_empty());
        CHECK_FALSE(triangulated.components[1].is_empty());

        const MeshMask extruded = mask::extrude(triangulated, glm::dvec2(90.0, 110.0));
        REQUIRE(extruded.components.size() == 2);
        for (const SimpleMesh &component : extruded.components) {
            check_closed_volume(component);
        }
    }
}

TEST_CASE("multi-component masks use union semantics") {
    const SimpleMesh source = make_plane();
    const MeshMask mask{
        .components = {
            make_box(glm::dvec3(-2.0, -2.0, -1.0), glm::dvec3(-1.0, 2.0, 1.0)),
            make_box(glm::dvec3(1.0, -2.0, -1.0), glm::dvec3(2.0, 2.0, 1.0)),
        }};

    const Cow<const SimpleMesh> inside = clip_on_mask(source, mask, true);
    REQUIRE_FALSE(inside->is_empty());
    CHECK(area(inside.get()) == Catch::Approx(4.0));

    const Cow<const SimpleMesh> outside = clip_on_mask(source, mask, false);
    REQUIRE_FALSE(outside->is_empty());
    CHECK(area(outside.get()) == Catch::Approx(8.0));
}

TEST_CASE("empty component list has empty-union semantics") {
    const SimpleMesh source = make_plane();
    const MeshMask mask;

    const Cow<const SimpleMesh> inside = clip_on_mask(source, mask, true);
    CHECK_FALSE(inside.is_ref());
    CHECK(inside->is_empty());

    const Cow<const SimpleMesh> outside = clip_on_mask(source, mask, false);
    CHECK(outside.is_ref());
    CHECK_FALSE(outside->is_empty());
}

TEST_CASE("Tirol mask components remain valid after node-bounds clipping") {
    const std::filesystem::path mask_path =
        std::filesystem::path(ALP_TEST_DATA_DIR) / "sf_builder_merge_border/mask/tirol.shp";
    const glm::dvec2 radius_range = mask::pad_radius_range(earth::radius_range(), 2.0);
    const auto loaded = mask::load_from_path(mask_path, radius_range);
    REQUIRE(loaded.has_value());

    const MeshMask &full_mask = loaded.value();
    REQUIRE(full_mask.components.size() == 3);
    for (const SimpleMesh &component : full_mask.components) {
        check_closed_volume(component);
    }

    const octree::Id regression_node(15, 26290, 18610, 27235);
    const radix::geometry::Aabb3d bounds = geometry::pad_bounds_relative(
        octree::Space::earth().get_node_bounds(regression_node), 0.05);
    const MeshMask local_mask = clip_mask_on_bounds(full_mask, bounds);
    REQUIRE_FALSE(local_mask.is_empty());
    for (const SimpleMesh &component : local_mask.components) {
        check_closed_volume(component);
    }
}

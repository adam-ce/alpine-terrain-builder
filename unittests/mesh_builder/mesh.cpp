/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 Adam Celarek <last name at cg tuwien ac at>
 * Copyright (C) 2022 alpinemaps.org
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <filesystem>
#include <unordered_set>

#include <fmt/core.h>

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/border.h>
#include <CGAL/Polygon_mesh_processing/connected_components.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Surface_mesh/Surface_mesh.h>
#include <CGAL/Unique_hash_map.h>

#include "../catch2_helpers.h"
#include "Dataset.h"
#include "mesh/merge.h"
#include "mesh/SimpleMesh.h"
#include "mesh_builder.h"
#include "mesh/io.h"
#include "mesh/cgal.h"
#include "mesh/cleanup.h"
#include "mesh/geometry.h"
#include "mesh/topology/connected_components.h"
#include "octree/Id.h"
#include "octree/Space.h"
#include "srs.h"
#include "mesh/cgal.h"
#include "mesh/convert.h"

void check_mesh_basics(const SimpleMesh &mesh) {
    const cgal::Mesh cgal_mesh = convert::to_cgal_mesh(mesh);
    CHECK(cgal_mesh.is_valid(true));
    CHECK(CGAL::is_triangle_mesh(cgal_mesh));
    CHECK(CGAL::is_valid_polygon_mesh(cgal_mesh, true));
    CHECK_FALSE(CGAL::Polygon_mesh_processing::does_self_intersect(cgal_mesh));
}

void check_no_holes(const SimpleMesh &mesh) {
    const cgal::Mesh cgal_mesh = convert::to_cgal_mesh(mesh);
    std::vector<cgal::HalfedgeDescriptor> border_cycles;
    CGAL::Polygon_mesh_processing::extract_boundary_cycles(cgal_mesh, std::back_inserter(border_cycles));
    const size_t nb_holes = border_cycles.size() - 1; // outer edge is a boundary cycle
    CHECK(nb_holes == 0);
}

void check_uvs(const SimpleMesh &mesh) {
    CHECK(mesh.uvs.size() == mesh.positions.size());

    for (const glm::dvec2 uv : mesh.uvs) {
        REQUIRE(glm::all(glm::greaterThanEqual(uv, glm::dvec2(0))));
        REQUIRE(glm::all(glm::lessThanEqual(uv, glm::dvec2(1))));
    }
}

void check_non_empty(const SimpleMesh &mesh) {
    CHECK(mesh.positions.size() > 0);
    CHECK(mesh.triangles.size() > 0);
}

struct DVec3Hash {
    size_t operator()(const glm::dvec3 &v) const {
        return hash::combine(v.x, v.y, v.z);
    }
};

struct DVec3Equal {
    bool operator()(const glm::dvec3 &a, const glm::dvec3 &b) const {
        return glm::all(glm::epsilonEqual(a, b, 1e-8));
    }
};

void check_duplicate_vertices(const std::vector<glm::dvec3> &positions) {
    std::unordered_set<glm::dvec3, DVec3Hash, DVec3Equal> seen;
    for (const auto &pos : positions) {
        REQUIRE(seen.insert(pos).second);
    }
}

void check_duplicate_triangles(const SimpleMesh& mesh) {
    const auto duplicate_triangles = mesh::find_duplicate_triangles(mesh, true);
    CHECK(duplicate_triangles == std::vector<uint32_t>{});
}

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> bounds_around_point(const glm::vec<n_dims, T> centre, const T margin) {
    radix::geometry::Aabb<n_dims, T> bounds;
    bounds.min = centre - glm::vec<n_dims, T>(margin);
    bounds.max = centre + glm::vec<n_dims, T>(margin);
    return bounds;
}

radix::geometry::Aabb3d extend_bounds_to_3d(radix::geometry::Aabb2d bounds2d) {
    const double infinity = std::numeric_limits<double>::infinity();
    const glm::dvec3 min(bounds2d.min, -infinity);
    const glm::dvec3 max(bounds2d.max, infinity);
    return radix::geometry::Aabb3d(min, max);
}

TEST_CASE("can build reference mesh patches for various datasets", "[terrainbuilder]") {
    struct TestData {
        std::string path_suffix;
        radix::geometry::Aabb3d target_bounds;
        OGRSpatialReference target_srs;
        OGRSpatialReference mesh_srs;
        double resolution; // in m
    };

    const auto mgi_srs = srs::mgi();
    const auto ecef_srs = srs::ecef();
    const auto wgs84_srs = srs::wgs84();
    const auto webmercator_srs = srs::webmercator();

    const glm::dvec3 pizbuin_summit_wgs84(10.118333, 46.844167, 3312);
    const glm::dvec3 pizbuin_summit_ecef = srs::transform_point(wgs84_srs, ecef_srs, pizbuin_summit_wgs84);
    const glm::dvec3 steffl_wgs84(16.3735655, 48.2083264, 204);
    const glm::dvec3 steffl_ecef = srs::transform_point(wgs84_srs, ecef_srs, steffl_wgs84);

    const std::vector<TestData> test_data{
        {"/austria/pizbuin_1m_epsg4326.tif",
         extend_bounds_to_3d(bounds_around_point(glm::dvec2(pizbuin_summit_wgs84), 0.0001)),
         wgs84_srs,
         wgs84_srs,
         1},
        {"/austria/pizbuin_1m_mgi.tif",
         bounds_around_point(pizbuin_summit_ecef, 50 * 1.),
         ecef_srs,
         ecef_srs,
         1},
        {"/austria/vienna_20m_mgi.tif",
         bounds_around_point(steffl_ecef, 50 * 20.),
         ecef_srs,
         ecef_srs,
         20},
        {"/austria/at_100m_mgi.tif",
         bounds_around_point(steffl_ecef, 50 * 100.),
         ecef_srs,
         ecef_srs,
         100}};

    for (const auto &data : test_data) {
        DYNAMIC_SECTION(data.path_suffix) {
            Dataset dataset(std::filesystem::path(ATB_TEST_DATA_DIR).concat(data.path_suffix));
            const auto source_srs = dataset.srs();
            const auto &mesh_srs = data.mesh_srs;
            const auto &target_srs = data.target_srs;
            const auto &target_bounds = data.target_bounds;
            const auto resolution = data.resolution;

            radix::tile::SrsBounds texture_bounds;
            const auto result = terrainbuilder::build_reference_mesh_patch(
                dataset,
                mesh_srs,
                target_srs, target_bounds,
                source_srs, texture_bounds);
            if (!result) {
                FAIL("Failed to build mesh: " << result.error());
            }
            const SimpleMesh mesh = result.value();

            SECTION("Basic mesh properties") {
                check_non_empty(mesh);
                check_uvs(mesh);
                check_duplicate_vertices(mesh.positions);
                check_duplicate_triangles(mesh);
                check_mesh_basics(mesh);
            }

            const std::vector<glm::dvec3> positions_in_target_srs = srs::transform_points(mesh_srs, target_srs, mesh.positions);
            SECTION("Vertices within target bounds") {
                for (const auto &position : positions_in_target_srs) {
                    REQUIRE(radix::geometry::Aabb2d(target_bounds).contains_inclusive(glm::dvec2(position)));
                    REQUIRE(target_bounds.contains_inclusive(position));
                }
            }

            SECTION("Some vertices on bounds (clipping check)") {
                std::vector<glm::dvec3> vertices_on_bounds;
                for (const glm::dvec3 position : positions_in_target_srs) {
                    if (!target_bounds.contains_exclusive(position)) {
                        vertices_on_bounds.push_back(position);
                    }
                }
                CHECK(vertices_on_bounds.size() > 10);
            }

            SECTION("Matches dataset resolution") {
                const auto positions_in_source_srs = srs::transform_points(mesh_srs, source_srs, mesh.positions);
                auto flat_positions_in_source_srs = positions_in_source_srs;
                for (auto &pos : flat_positions_in_source_srs) {
                    pos.z = 0.0;
                }
                const auto flat_positions_in_ecef_srs = srs::transform_points(source_srs, ecef_srs, flat_positions_in_source_srs);

                const auto target_bounds_2d = radix::geometry::Aabb2d(target_bounds);
                const auto padding = target_bounds_2d.size() * 0.1;
                const auto inner_min = target_bounds_2d.min + padding;
                const auto inner_max = target_bounds_2d.max - padding;
                const radix::geometry::Aabb2d inner_bounds_2d{inner_min, inner_max};

                std::vector<glm::uvec3> filtered_triangles;
                for (const auto &tri : mesh.triangles) {
                    const glm::dvec3 &a = positions_in_target_srs[tri.x];
                    const glm::dvec3 &b = positions_in_target_srs[tri.y];
                    const glm::dvec3 &c = positions_in_target_srs[tri.z];
                    if (inner_bounds_2d.contains_inclusive(a) &&
                        inner_bounds_2d.contains_inclusive(b) &&
                        inner_bounds_2d.contains_inclusive(c)) {
                        filtered_triangles.push_back(tri);
                    }
                }
                SimpleMesh inside_flat_mesh;
                inside_flat_mesh.positions = flat_positions_in_ecef_srs;
                inside_flat_mesh.triangles = filtered_triangles;

                const auto avg_edge_length = mesh::estimate_average_edge_length(inside_flat_mesh);
                REQUIRE(avg_edge_length.has_value());
                const auto expected_avg_edge_length = ((1 + 1 + std::sqrt(3)) / 3) * resolution;
                CHECK(avg_edge_length.value() == Catch::Approx(expected_avg_edge_length).margin(expected_avg_edge_length * 0.2));
            }
        }
    }
}

TEST_CASE("neighbouring patches fit together", "[terrainbuilder]") {
    const auto mgi_srs = srs::mgi();
    const auto ecef_srs = srs::ecef();
    const auto wgs84_srs = srs::wgs84();
    const auto webmercator_srs = srs::webmercator();

    const glm::dvec3 pizbuin_summit_wgs84(10.118333, 46.844167, 3312);
    const glm::dvec3 pizbuin_summit_ecef = srs::transform_point(wgs84_srs, ecef_srs, pizbuin_summit_wgs84);
    const octree::Space space = octree::Space::earth();
    const octree::Id summit_node = space.find_node_at_level_containing_point(pizbuin_summit_ecef, 17).value();
    std::vector<octree::Id> nodes = summit_node.neighbours();
    nodes.push_back(summit_node);

    const std::string dataset_suffix = "/austria/pizbuin_1m_mgi.tif";
    const std::filesystem::path dataset_path = std::filesystem::path(ATB_TEST_DATA_DIR).concat(dataset_suffix);
    Dataset dataset(dataset_path);

    std::vector<SimpleMesh> node_meshes;
    for (const octree::Id &node : nodes) {
        const octree::Bounds node_bounds = space.get_node_bounds(node);
        radix::tile::SrsBounds output_texture_bounds;
        const auto result = terrainbuilder::build_reference_mesh_patch(
            dataset,
            ecef_srs,
            ecef_srs, node_bounds,
            webmercator_srs, output_texture_bounds);
        if (!result.has_value()) {
            continue;
        }
        const SimpleMesh mesh = result.value();
        node_meshes.push_back(mesh);
    }
    CHECK(node_meshes.size() >= 3);

    const SimpleMesh merged_mesh = mesh::merge(node_meshes, mesh::merging::create_options().epsilon(1e-6));
    check_mesh_basics(merged_mesh);
    check_non_empty(merged_mesh);
    check_no_holes(merged_mesh);
    CHECK(mesh::count_connected_components(merged_mesh) == 1);
}

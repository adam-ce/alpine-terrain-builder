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

#include <catch2/catch.hpp>
#include <fmt/core.h>

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/connected_components.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/stitch_borders.h>
#include <CGAL/Surface_mesh/Surface_mesh.h>
#include <CGAL/Unique_hash_map.h>

#include "../catch2_helpers.h"
#include "Dataset.h"
#include "ctb/GlobalMercator.hpp"
#include "ctb/GlobalGeodetic.hpp"
#include "ctb/Grid.hpp"
#include "srs.h"

#include "mesh/io.h"
#include "mesh_builder.h"
#include "mesh/terrain_mesh.h"
#include "merge.h"

typedef CGAL::Exact_predicates_inexact_constructions_kernel Kernel;
typedef Kernel::Point_3 Point3;
typedef CGAL::Surface_mesh<Point3> SurfaceMesh;
typedef boost::graph_traits<SurfaceMesh>::vertex_descriptor VertexDescriptor;
typedef boost::graph_traits<SurfaceMesh>::edge_descriptor EdgeDescriptor;
typedef boost::graph_traits<SurfaceMesh>::face_descriptor FaceDescriptor;

Point3 glm2cgal(glm::dvec3 point) {
    return Point3(point[0], point[1], point[2]);
}

SurfaceMesh mesh2cgal(const TerrainMesh &mesh) {
    SurfaceMesh cgal_mesh;

    for (const glm::dvec3 &position : mesh.positions) {
        const CGAL::SM_Vertex_index vertex = cgal_mesh.add_vertex(glm2cgal(position));
        REQUIRE(vertex != SurfaceMesh::null_vertex());
    }

    for (const glm::uvec3 &triangle : mesh.triangles) {
        const CGAL::SM_Face_index face = cgal_mesh.add_face(
            CGAL::SM_Vertex_index(triangle.x),
            CGAL::SM_Vertex_index(triangle.y),
            CGAL::SM_Vertex_index(triangle.z));

        REQUIRE(face != SurfaceMesh::null_face());
    }

    return cgal_mesh;
}

size_t count_connected_components(const SurfaceMesh &mesh) {
    typedef CGAL::Unique_hash_map<FaceDescriptor, size_t> CcMap;
    typedef boost::associative_property_map<CcMap> CcPropertyMap;

    CcMap cc_map;
    CcPropertyMap cc_pmap(cc_map);
    const size_t num = CGAL::Polygon_mesh_processing::connected_components(mesh, cc_pmap);
    return num;
}

glm::dvec3 apply_transform(OGRCoordinateTransformation *transform, const glm::dvec3 &v) {
    glm::dvec3 result(v);
    REQUIRE(transform->Transform(1, &result.x, &result.y, &result.z));
    return result;
}

void check_mesh_is_plane(const TerrainMesh &mesh) {
    const SurfaceMesh cgal_mesh = mesh2cgal(mesh);
    REQUIRE(cgal_mesh.is_valid(true));
    REQUIRE(CGAL::is_triangle_mesh(cgal_mesh));
    REQUIRE(CGAL::is_valid_polygon_mesh(cgal_mesh, true));
    REQUIRE(count_connected_components(cgal_mesh) == 1);
    REQUIRE(!CGAL::Polygon_mesh_processing::does_self_intersect(cgal_mesh));
}

void check_uvs(const TerrainMesh &mesh) {
    REQUIRE(mesh.uvs.size() == mesh.positions.size());

    for (const glm::dvec2 uv : mesh.uvs) {
        REQUIRE(glm::all(glm::greaterThanEqual(uv, glm::dvec2(0))));
        REQUIRE(glm::all(glm::lessThanEqual(uv, glm::dvec2(1))));
    }
}

void check_non_empty(const TerrainMesh &mesh) {
    REQUIRE(mesh.positions.size() > 0);
    REQUIRE(mesh.triangles.size() > 0);
}

TEST_CASE("can build reference mesh tiles", "[terrainbuilder]") {
    const std::vector<std::tuple<std::string, std::string, radix::tile::Id>> tile_test_cases = {
        {"tiny tile", "/austria/pizbuin_1m_epsg4326.tif", radix::tile::Id(23, glm::uvec2(4430412, 2955980), radix::tile::Scheme::SlippyMap)},
        {"small tile", "/austria/pizbuin_1m_epsg3857.tif", radix::tile::Id(20, glm::uvec2(553801, 369497), radix::tile::Scheme::SlippyMap)},
        {"tile on the border", "/austria/pizbuin_1m_mgi.tif", radix::tile::Id(18, glm::uvec2(138457, 169781), radix::tile::Scheme::Tms)}
#if defined(ATB_UNITTESTS_EXTENDED) && ATB_UNITTESTS_EXTENDED
        {"tile slightly larger than dataset", "/austria/pizbuin_1m_mgi.tif", radix::tile::Id(11, glm::uvec2(1081, 721), radix::tile::Scheme::SlippyMap)},
        {"huge tile", "/austria/pizbuin_1m_epsg3857.tif", radix::tile::Id(6, glm::uvec2(33, 41), radix::tile::Scheme::Tms)},
        {"giant tile", "/austria/at_mgi.tif", radix::tile::Id(1, glm::uvec2(1, 0), radix::tile::Scheme::SlippyMap)},
#endif
    };

    

    const ctb::Grid grid = ctb::GlobalMercator();
    std::vector<std::tuple<std::string, std::string, radix::tile::SrsBounds>> test_cases = {
        {"custom bounds", "/austria/pizbuin_1m_epsg4326.tif", radix::tile::SrsBounds(glm::dvec2(1127962, 5915858), glm::dvec2(1127966, 5915882))}};
    for (const auto &test : tile_test_cases) {
        const auto [test_name, dataset_suffix, target_tile] = test;
        const radix::tile::SrsBounds tile_bounds = grid.srsBounds(target_tile, false);
        test_cases.push_back({test_name, dataset_suffix, tile_bounds});
    }

    for (const auto &test : test_cases) {
        const auto [test_name, dataset_suffix, target_bounds] = test;

        DYNAMIC_SECTION(test_name) {
            const std::filesystem::path dataset_path = std::filesystem::path(ATB_TEST_DATA_DIR).concat(dataset_suffix);
            Dataset dataset(dataset_path);

            OGRSpatialReference webmercator_srs;
            webmercator_srs.importFromEPSG(3857);
            webmercator_srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            OGRSpatialReference ecef_srs;
            ecef_srs.importFromEPSG(4978);
            ecef_srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

            radix::tile::SrsBounds output_tile_bounds;
            radix::tile::SrsBounds output_texture_bounds;

            output_tile_bounds = target_bounds;
            output_texture_bounds = target_bounds;
            const TerrainMesh mesh = terrainbuilder::mesh::build_reference_mesh_tile(
                                         dataset,
                                         ecef_srs,
                                         grid.getSRS(), output_tile_bounds,
                                         webmercator_srs, output_texture_bounds,
                                         terrainbuilder::Border(0),
                                         false)
                                         .value();

            check_non_empty(mesh);
            check_uvs(mesh);
            check_mesh_is_plane(mesh);

            // check all vertices inside bounds
            const std::unique_ptr<OGRCoordinateTransformation> transform_ecef_webmercator = srs::transformation(ecef_srs, webmercator_srs);
            for (const glm::dvec3 ecef_position : mesh.positions) {
                const glm::dvec3 webmercator_position = apply_transform(transform_ecef_webmercator.get(), ecef_position);
                REQUIRE(target_bounds.contains_inclusive(webmercator_position));
            }

            // TODO: also test inclusive bounds (but this requires reconstructing the quads of the mesh)
        }
    }
}

TEST_CASE("neighbouring tiles fit together", "[terrainbuilder]") {
    const ctb::Grid grid = ctb::GlobalMercator();
    const std::string dataset_suffix = "/austria/pizbuin_1m_epsg3857.tif";
    const std::array<radix::tile::Id, 4> tiles = radix::tile::Id(20, glm::uvec2(553801, 369497), radix::tile::Scheme::SlippyMap).children();

    std::vector<TerrainMesh> tile_meshes;
    for (const radix::tile::Id &tile : tiles) {
        const radix::tile::SrsBounds tile_bounds = grid.srsBounds(tile, false);
        DYNAMIC_SECTION(tile) {
            const std::filesystem::path dataset_path = std::filesystem::path(ATB_TEST_DATA_DIR).concat(dataset_suffix);
            Dataset dataset(dataset_path);

            OGRSpatialReference webmercator_srs;
            webmercator_srs.importFromEPSG(3857);
            webmercator_srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            OGRSpatialReference ecef_srs;
            ecef_srs.importFromEPSG(4978);
            ecef_srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

            radix::tile::SrsBounds output_tile_bounds;
            radix::tile::SrsBounds output_texture_bounds;

            output_tile_bounds = tile_bounds;
            output_texture_bounds = tile_bounds;
            const TerrainMesh mesh = terrainbuilder::mesh::build_reference_mesh_tile(
                dataset,
                ecef_srs,
                grid.getSRS(), output_tile_bounds,
                webmercator_srs, output_texture_bounds,
                terrainbuilder::Border(0, 1, 1, 0),
                true).value();

            tile_meshes.push_back(mesh);
        }
    }

    const TerrainMesh merged_mesh = merge::merge_meshes(tile_meshes, 0.1);
    check_non_empty(merged_mesh);
    check_mesh_is_plane(merged_mesh);
}

TEST_CASE("neighbouring tiles fit together repeatedly", "[terrainbuilder]") {
    const ctb::Grid grid = ctb::GlobalMercator();
    const std::string dataset_suffix = "/austria/innenstadt_gs_1m_mgi.tif";

    const radix::tile::Id root_tile_id(16, glm::uvec2(35748, 22724), radix::tile::Scheme::SlippyMap);
    std::vector<TerrainMesh> child_meshes;
    for (const radix::tile::Id child_tile_id : root_tile_id.children()) {
        std::vector<TerrainMesh> grand_child_meshes;
        for (const radix::tile::Id grand_child_tile_id : child_tile_id.children()) {
            const radix::tile::SrsBounds grand_child_tile_bounds = grid.srsBounds(grand_child_tile_id, false);
            DYNAMIC_SECTION(grand_child_tile_id) {
                const std::filesystem::path dataset_path = std::filesystem::path(ATB_TEST_DATA_DIR).concat(dataset_suffix);
                Dataset dataset(dataset_path);

                OGRSpatialReference webmercator_srs;
                webmercator_srs.importFromEPSG(3857);
                webmercator_srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                OGRSpatialReference ecef_srs;
                ecef_srs.importFromEPSG(4978);
                ecef_srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

                radix::tile::SrsBounds output_tile_bounds;
                radix::tile::SrsBounds output_texture_bounds;

                output_tile_bounds = grand_child_tile_bounds;
                output_texture_bounds = grand_child_tile_bounds;
                const TerrainMesh grand_child_mesh = terrainbuilder::mesh::build_reference_mesh_tile(
                    dataset,
                    ecef_srs,
                    grid.getSRS(), output_tile_bounds,
                    webmercator_srs, output_texture_bounds,
                    terrainbuilder::Border(0, 1, 1, 0),
                    true).value();

                grand_child_meshes.push_back(grand_child_mesh);
            }
        }

        const TerrainMesh child_mesh = merge::merge_meshes(grand_child_meshes, 0.1);
        child_meshes.push_back(child_mesh);
    }

    const TerrainMesh mesh = merge::merge_meshes(child_meshes, 0.1);
    check_non_empty(mesh);
    check_mesh_is_plane(mesh);
}

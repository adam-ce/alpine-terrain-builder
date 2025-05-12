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

#include <opencv2/opencv.hpp>
#include <fmt/core.h>

#include "../catch2_helpers.h"
#include "mesh/io.h"

// modified from https://stackoverflow.com/a/32440830/6304917
bool mat_equals(const cv::Mat mat1, const cv::Mat mat2) {
    if (mat1.dims != mat2.dims ||
        mat1.size != mat2.size ||
        mat1.elemSize() != mat2.elemSize()) {
        return false;
    }

    if (mat1.isContinuous() && mat2.isContinuous()) {
        return std::memcmp(mat1.ptr(), mat2.ptr(), mat1.total() * mat1.elemSize()) == 0;
    } else {
        const cv::Mat *arrays[] = {&mat1, &mat2, 0};
        uchar *ptrs[2];
        cv::NAryMatIterator it(arrays, ptrs, 2);
        for (unsigned int p = 0; p < it.nplanes; p++, ++it) {
            if (memcmp(it.ptrs[0], it.ptrs[1], it.size * mat1.elemSize()) != 0) {
                return false;
            }
        }

        return true;
    }
}

TEST_CASE("io roundtrip") {
    for (const auto& format : {"glb", "gltf"}) {
        DYNAMIC_SECTION(format) {
            SimpleMesh mesh;

            mesh.positions.push_back(glm::dvec3(0, 0, 0));
            mesh.positions.push_back(glm::dvec3(1, 0, 0));
            mesh.positions.push_back(glm::dvec3(0, 1, 0));
            mesh.positions.push_back(glm::dvec3(1, 1, 0));

            mesh.triangles.push_back(glm::uvec3(0, 2, 1));
            mesh.triangles.push_back(glm::uvec3(1, 2, 3));

            mesh.uvs.push_back(glm::dvec2(0, 0));
            mesh.uvs.push_back(glm::dvec2(1, 0));
            mesh.uvs.push_back(glm::dvec2(0, 1));
            mesh.uvs.push_back(glm::dvec2(1, 1));

            mesh.texture = cv::Mat3b(100, 100);
            cv::randu(*mesh.texture, cv::Scalar(0, 0, 0), cv::Scalar(256, 256, 256));

            const std::filesystem::path mesh_path = fmt::format("./unittests/output/mesh.{}", format);
            std::filesystem::remove(mesh_path);
            REQUIRE(!std::filesystem::exists(mesh_path));

            mesh::io::save_to_path(mesh, mesh_path, mesh::io::SaveOptions{.texture_format = ".png"});
            REQUIRE(std::filesystem::exists(mesh_path));

            const tl::expected<SimpleMesh, mesh::io::LoadMeshError> result = mesh::io::load_from_path(mesh_path);
            if (!result.has_value()) {
                FAIL(result.error().description());
            }
            std::filesystem::remove(mesh_path);
            const SimpleMesh roundtrip_mesh = result.value();
            REQUIRE(roundtrip_mesh.positions == mesh.positions);
            REQUIRE(roundtrip_mesh.uvs == mesh.uvs);
            REQUIRE(roundtrip_mesh.triangles == mesh.triangles);
            REQUIRE(roundtrip_mesh.texture.has_value());
            REQUIRE(mat_equals(*roundtrip_mesh.texture, *mesh.texture));
        }
    }
}

TEST_CASE("io roundtrip high precision") {
    for (const auto& format : {"terrain"}) {
        DYNAMIC_SECTION(format) {
            SimpleMesh mesh;

            const double pi = std::numbers::pi_v<double>;
            REQUIRE((double)(float)pi != pi);

            mesh.positions.push_back(glm::dvec3(0, 0, 0));
            mesh.positions.push_back(glm::dvec3(pi, 0, 0));
            mesh.positions.push_back(glm::dvec3(0, pi, 0));

            mesh.triangles.push_back(glm::uvec3(0, 2, 1));

            mesh.uvs.push_back(glm::dvec2(0, 0));
            mesh.uvs.push_back(glm::dvec2(1, 0));
            mesh.uvs.push_back(glm::dvec2(0, 1));

            mesh.texture = cv::Mat3b(100, 100);
            cv::randu(*mesh.texture, cv::Scalar(0, 0, 0), cv::Scalar(256, 256, 256));

            const std::filesystem::path mesh_path = fmt::format("./unittests/output/mesh.{}", format);
            std::filesystem::remove(mesh_path);
            REQUIRE(!std::filesystem::exists(mesh_path));

            mesh::io::save_to_path(mesh, mesh_path, mesh::io::SaveOptions{.texture_format = ".png"});
            REQUIRE(std::filesystem::exists(mesh_path));

            const tl::expected<SimpleMesh, mesh::io::LoadMeshError> result = mesh::io::load_from_path(mesh_path);
            if (!result.has_value()) {
                FAIL(result.error().description());
            }
            std::filesystem::remove(mesh_path);
            const SimpleMesh roundtrip_mesh = result.value();
            REQUIRE(roundtrip_mesh.positions == mesh.positions);
            REQUIRE(roundtrip_mesh.uvs == mesh.uvs);
            REQUIRE(roundtrip_mesh.triangles == mesh.triangles);
            REQUIRE(roundtrip_mesh.texture.has_value());
            REQUIRE(mat_equals(*roundtrip_mesh.texture, *mesh.texture));
        }
    }
}

TEST_CASE("io roundtrip no texture") {
    for (const auto &format : {"gltf", "glb", "terrain"}) {
        DYNAMIC_SECTION(format) {
            SimpleMesh mesh;

            const double pi = std::numbers::pi_v<double>;
            REQUIRE((double)(float)pi != pi);

            mesh.positions.push_back(glm::dvec3(0, 0, 0));
            mesh.positions.push_back(glm::dvec3(1, 0, 0));
            mesh.positions.push_back(glm::dvec3(0, 1, 0));
            mesh.positions.push_back(glm::dvec3(1, 1, 0));

            mesh.triangles.push_back(glm::uvec3(0, 2, 1));
            mesh.triangles.push_back(glm::uvec3(1, 2, 3));

            mesh.uvs.push_back(glm::dvec2(0, 0));
            mesh.uvs.push_back(glm::dvec2(1, 0));
            mesh.uvs.push_back(glm::dvec2(0, 1));
            mesh.uvs.push_back(glm::dvec2(1, 1));

            const std::filesystem::path mesh_path = fmt::format("./unittests/output/mesh.{}", format);
            std::filesystem::remove(mesh_path);
            REQUIRE(!std::filesystem::exists(mesh_path));

            mesh::io::save_to_path(mesh, mesh_path);
            REQUIRE(std::filesystem::exists(mesh_path));

            const tl::expected<SimpleMesh, mesh::io::LoadMeshError> result = mesh::io::load_from_path(mesh_path);
            if (!result.has_value()) {
                FAIL(result.error().description());
            }
            // std::filesystem::remove(mesh_path);
            const SimpleMesh roundtrip_mesh = result.value();
            REQUIRE(roundtrip_mesh.positions == mesh.positions);
            REQUIRE(roundtrip_mesh.uvs == mesh.uvs);
            REQUIRE(roundtrip_mesh.triangles == mesh.triangles);
            REQUIRE(!roundtrip_mesh.texture.has_value());
        }
    }
}

TEST_CASE("io roundtrip no texture and uvs") {
    for (const auto &format : {"gltf", "glb", "terrain"}) {
        DYNAMIC_SECTION(format) {
            SimpleMesh mesh;

            const double pi = std::numbers::pi_v<double>;
            REQUIRE((double)(float)pi != pi);

            mesh.positions.push_back(glm::dvec3(0, 0, 0));
            mesh.positions.push_back(glm::dvec3(1, 0, 0));
            mesh.positions.push_back(glm::dvec3(0, 1, 0));
            mesh.positions.push_back(glm::dvec3(1, 1, 0));

            mesh.triangles.push_back(glm::uvec3(0, 2, 1));
            mesh.triangles.push_back(glm::uvec3(1, 2, 3));

            const std::filesystem::path mesh_path = fmt::format("./unittests/output/mesh.{}", format);
            std::filesystem::remove(mesh_path);
            REQUIRE(!std::filesystem::exists(mesh_path));

            mesh::io::save_to_path(mesh, mesh_path);
            REQUIRE(std::filesystem::exists(mesh_path));

            const tl::expected<SimpleMesh, mesh::io::LoadMeshError> result = mesh::io::load_from_path(mesh_path);
            if (!result.has_value()) {
                FAIL(result.error().description());
            }
            std::filesystem::remove(mesh_path);
            const SimpleMesh roundtrip_mesh = result.value();
            REQUIRE(roundtrip_mesh.positions == mesh.positions);
            REQUIRE(!roundtrip_mesh.has_uvs());
            REQUIRE(roundtrip_mesh.triangles == mesh.triangles);
            REQUIRE(!roundtrip_mesh.texture.has_value());
        }
    }
}

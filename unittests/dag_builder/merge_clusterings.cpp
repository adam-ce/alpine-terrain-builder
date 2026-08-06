#include "../catch2_helpers.h"

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "cluster.h"
#include "merge/clusterings.h"

namespace {

constexpr double position_tolerance = 1e-9;

cv::Mat make_texture(const uchar tag) {
    cv::Mat texture(1, 1, CV_8UC3);
    texture.at<cv::Vec3b>(0, 0) = cv::Vec3b(tag, tag, tag);
    return texture;
}

// A single, unconnected triangle. Since none of its edges have a twin, all
// three vertices are boundary vertices and are eligible for welding.
Clustering make_triangle_clustering(
    const glm::dvec3 &v0,
    const glm::dvec3 &v1,
    const glm::dvec3 &v2,
    const uchar texture_tag = 0) {
    Clustering clustering;
    clustering.positions = {v0, v1, v2};
    clustering.textures.add(make_texture(texture_tag));

    Cluster cluster;
    cluster.id = 0;
    cluster.vertex_indices = {0, 1, 2};
    cluster.local_triangles = {glm::uvec3(0, 1, 2)};
    cluster.uvs = {glm::dvec2(0.0, 0.0), glm::dvec2(1.0, 0.0), glm::dvec2(0.0, 1.0)};
    cluster.texture_id = 0;
    clustering.clusters.push_back(std::move(cluster));

    return clustering;
}

// A closed fan of four triangles around a center vertex. Every edge touching
// the center vertex is shared by two triangles (and thus has a twin), so the
// center vertex is interior. Each outer vertex touches an unmatched outer edge
// and is therefore a boundary vertex.
Clustering make_fan_clustering(
    const glm::dvec3 &center,
    const glm::dvec3 &outer0,
    const glm::dvec3 &outer1,
    const glm::dvec3 &outer2,
    const glm::dvec3 &outer3,
    const uchar texture_tag = 0) {
    Clustering clustering;
    clustering.positions = {outer0, outer1, outer2, outer3, center};
    clustering.textures.add(make_texture(texture_tag));

    Cluster cluster;
    cluster.id = 0;
    cluster.vertex_indices = {0, 1, 2, 3, 4};
    cluster.local_triangles = {
        glm::uvec3(4, 0, 1),
        glm::uvec3(4, 1, 2),
        glm::uvec3(4, 2, 3),
        glm::uvec3(4, 3, 0),
    };
    cluster.uvs = {
        glm::dvec2(0.0, 0.0),
        glm::dvec2(1.0, 0.0),
        glm::dvec2(1.0, 1.0),
        glm::dvec2(0.0, 1.0),
        glm::dvec2(0.5, 0.5),
    };
    cluster.texture_id = 0;
    clustering.clusters.push_back(std::move(cluster));

    return clustering;
}

std::size_t global_vertex_index(
    const Clustering &clustering,
    const std::size_t cluster_index,
    const std::size_t local_vertex_index) {
    return static_cast<std::size_t>(
        clustering.clusters.at(cluster_index).vertex_indices.at(local_vertex_index));
}

const glm::dvec3 &position_for_local_vertex(
    const Clustering &clustering,
    const std::size_t cluster_index,
    const std::size_t local_vertex_index) {
    return clustering.positions.at(
        global_vertex_index(clustering, cluster_index, local_vertex_index));
}

void check_position_near(
    const glm::dvec3 &actual,
    const glm::dvec3 &expected,
    const double tolerance = position_tolerance) {
    CHECK(glm::distance(actual, expected) <= tolerance);
}

void check_triangle_cluster_topology(const Cluster &cluster) {
    REQUIRE(cluster.vertex_indices.size() == 3);
    CHECK(cluster.local_triangles == std::vector<glm::uvec3>{glm::uvec3(0, 1, 2)});
}

} // namespace

TEST_CASE(
    "merge_clusterings returns an empty clustering for empty input",
    "[dag_builder][merge]") {
    const std::vector<Clustering> clusterings;
    const Clustering merged = merge_clusterings(clusterings, 0.01);

    CHECK(merged.positions.empty());
    CHECK(merged.clusters.empty());
}

TEST_CASE(
    "merge_clusterings returns a copy of the single input clustering unchanged",
    "[dag_builder][merge]") {
    const Clustering clustering = make_triangle_clustering(
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(0.0, 1.0, 0.0));

    const std::vector<Clustering> clusterings = {clustering};
    const Clustering merged = merge_clusterings(clusterings, 0.01);

    REQUIRE(merged.positions.size() == clustering.positions.size());
    for (std::size_t i = 0; i < clustering.positions.size(); ++i) {
        CHECK(merged.positions[i] == clustering.positions[i]);
    }

    REQUIRE(merged.clusters.size() == clustering.clusters.size());
    CHECK(merged.clusters[0].id == clustering.clusters[0].id);
    CHECK(merged.clusters[0].texture_id == clustering.clusters[0].texture_id);
    CHECK(merged.clusters[0].vertex_indices == clustering.clusters[0].vertex_indices);
    CHECK(merged.clusters[0].local_triangles == clustering.clusters[0].local_triangles);
}

TEST_CASE(
    "merge_clusterings throws std::invalid_argument for non-positive or non-finite epsilon",
    "[dag_builder][merge]") {
    const std::vector<Clustering> clusterings = {
        make_triangle_clustering(
            glm::dvec3(0.0, 0.0, 0.0),
            glm::dvec3(10.0, 0.0, 0.0),
            glm::dvec3(5.0, 10.0, 0.0)),
        make_triangle_clustering(
            glm::dvec3(0.001, 0.0, 0.0),
            glm::dvec3(110.0, 0.0, 0.0),
            glm::dvec3(105.0, 10.0, 0.0)),
    };

    CHECK_THROWS_AS(merge_clusterings(clusterings, 0.0), std::invalid_argument);
    CHECK_THROWS_AS(merge_clusterings(clusterings, -1.0), std::invalid_argument);
    CHECK_THROWS_AS(
        merge_clusterings(clusterings, std::numeric_limits<double>::quiet_NaN()),
        std::invalid_argument);
    CHECK_THROWS_AS(
        merge_clusterings(clusterings, std::numeric_limits<double>::infinity()),
        std::invalid_argument);
}

TEST_CASE(
    "merge_clusterings welds boundary vertices within epsilon and remaps both clusters",
    "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(
        MergeMode::GreedyLocal,
        MergeMode::ConnectedComponents,
        MergeMode::MultipartiteNearest);

    const Clustering a = make_triangle_clustering(
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(10.0, 0.0, 0.0),
        glm::dvec3(5.0, 10.0, 0.0));
    const Clustering b = make_triangle_clustering(
        glm::dvec3(0.001, 0.0, 0.0),
        glm::dvec3(110.0, 0.0, 0.0),
        glm::dvec3(105.0, 10.0, 0.0));

    const std::vector<Clustering> clusterings = {a, b};
    const Clustering merged = merge_clusterings(
        clusterings,
        0.01,
        MergeOptions{.mode = merge_mode, .average_positions = true});

    REQUIRE(merged.clusters.size() == 2);
    REQUIRE(merged.vertex_count() == 5);
    check_triangle_cluster_topology(merged.clusters[0]);
    check_triangle_cluster_topology(merged.clusters[1]);

    const std::size_t welded_index_a = global_vertex_index(merged, 0, 0);
    const std::size_t welded_index_b = global_vertex_index(merged, 1, 0);
    CHECK(welded_index_a == welded_index_b);

    check_position_near(
        position_for_local_vertex(merged, 0, 0),
        glm::dvec3(0.0005, 0.0, 0.0));
    check_position_near(
        position_for_local_vertex(merged, 1, 0),
        glm::dvec3(0.0005, 0.0, 0.0));

    check_position_near(
        position_for_local_vertex(merged, 0, 1),
        glm::dvec3(10.0, 0.0, 0.0));
    check_position_near(
        position_for_local_vertex(merged, 1, 1),
        glm::dvec3(110.0, 0.0, 0.0));
}

TEST_CASE(
    "merge_clusterings does not weld boundary vertices further apart than epsilon",
    "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(
        MergeMode::GreedyLocal,
        MergeMode::ConnectedComponents,
        MergeMode::MultipartiteNearest);

    const Clustering a = make_triangle_clustering(
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(10.0, 0.0, 0.0),
        glm::dvec3(5.0, 10.0, 0.0));
    const Clustering b = make_triangle_clustering(
        glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(110.0, 0.0, 0.0),
        glm::dvec3(105.0, 10.0, 0.0));

    const std::vector<Clustering> clusterings = {a, b};
    const Clustering merged = merge_clusterings(
        clusterings,
        0.01,
        MergeOptions{.mode = merge_mode});

    REQUIRE(merged.clusters.size() == 2);
    REQUIRE(merged.vertex_count() == 6);
    CHECK(global_vertex_index(merged, 0, 0) != global_vertex_index(merged, 1, 0));

    check_position_near(
        position_for_local_vertex(merged, 0, 0),
        glm::dvec3(0.0, 0.0, 0.0));
    check_position_near(
        position_for_local_vertex(merged, 1, 0),
        glm::dvec3(1.0, 0.0, 0.0));
}

TEST_CASE(
    "merge_clusterings maps mutually close boundary vertices from three clusterings to one vertex",
    "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(
        MergeMode::GreedyLocal,
        MergeMode::ConnectedComponents,
        MergeMode::MultipartiteNearest);

    const Clustering a = make_triangle_clustering(
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(10.0, 0.0, 0.0),
        glm::dvec3(5.0, 10.0, 0.0));
    const Clustering b = make_triangle_clustering(
        glm::dvec3(0.001, 0.0, 0.0),
        glm::dvec3(110.0, 0.0, 0.0),
        glm::dvec3(105.0, 10.0, 0.0));
    const Clustering c = make_triangle_clustering(
        glm::dvec3(-0.001, 0.0, 0.0),
        glm::dvec3(210.0, 0.0, 0.0),
        glm::dvec3(205.0, 10.0, 0.0));

    const std::vector<Clustering> clusterings = {a, b, c};
    const Clustering merged = merge_clusterings(
        clusterings,
        0.01,
        MergeOptions{.mode = merge_mode});

    REQUIRE(merged.clusters.size() == 3);
    REQUIRE(merged.vertex_count() == 7);

    for (const Cluster &cluster : merged.clusters) {
        check_triangle_cluster_topology(cluster);
    }

    const std::size_t welded_index = global_vertex_index(merged, 0, 0);
    CHECK(global_vertex_index(merged, 1, 0) == welded_index);
    CHECK(global_vertex_index(merged, 2, 0) == welded_index);
}

TEST_CASE(
    "merge_clusterings averages an asymmetric three-clustering weld",
    "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(
        MergeMode::GreedyLocal,
        MergeMode::ConnectedComponents,
        MergeMode::MultipartiteNearest);

    const Clustering a = make_triangle_clustering(
        glm::dvec3(0.000, 0.0, 0.0),
        glm::dvec3(10.0, 0.0, 0.0),
        glm::dvec3(5.0, 10.0, 0.0));
    const Clustering b = make_triangle_clustering(
        glm::dvec3(0.003, 0.0, 0.0),
        glm::dvec3(110.0, 0.0, 0.0),
        glm::dvec3(105.0, 10.0, 0.0));
    const Clustering c = make_triangle_clustering(
        glm::dvec3(0.009, 0.0, 0.0),
        glm::dvec3(210.0, 0.0, 0.0),
        glm::dvec3(205.0, 10.0, 0.0));

    const std::vector<Clustering> clusterings = {a, b, c};
    const Clustering merged = merge_clusterings(
        clusterings,
        0.01,
        MergeOptions{.mode = merge_mode, .average_positions = true});

    REQUIRE(merged.vertex_count() == 7);

    const std::size_t welded_index = global_vertex_index(merged, 0, 0);
    CHECK(global_vertex_index(merged, 1, 0) == welded_index);
    CHECK(global_vertex_index(merged, 2, 0) == welded_index);
    check_position_near(merged.positions[welded_index], glm::dvec3(0.004, 0.0, 0.0));
}

TEST_CASE(
    "merge_clusterings welds boundary vertices but never coincident interior vertices",
    "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(
        MergeMode::GreedyLocal,
        MergeMode::ConnectedComponents,
        MergeMode::MultipartiteNearest);

    const glm::dvec3 center(0.0, 0.0, 0.0);
    const Clustering a = make_fan_clustering(
        center,
        glm::dvec3(10.000, 0.0, 0.0),
        glm::dvec3(0.0, 10.0, 0.0),
        glm::dvec3(-10.0, 0.0, 0.0),
        glm::dvec3(0.0, -10.0, 0.0));
    const Clustering b = make_fan_clustering(
        center,
        glm::dvec3(10.001, 0.0, 0.0),
        glm::dvec3(500.0, 10.0, 0.0),
        glm::dvec3(490.0, 0.0, 0.0),
        glm::dvec3(500.0, -10.0, 0.0));

    const std::vector<Clustering> clusterings = {a, b};
    const Clustering merged = merge_clusterings(
        clusterings,
        0.01,
        MergeOptions{.mode = merge_mode});

    REQUIRE(merged.clusters.size() == 2);
    REQUIRE(merged.vertex_count() == 9);

    // Local vertex 0 is on the boundary and must weld.
    CHECK(global_vertex_index(merged, 0, 0) == global_vertex_index(merged, 1, 0));

    // Local vertex 4 is the coincident interior center and must remain separate.
    CHECK(global_vertex_index(merged, 0, 4) != global_vertex_index(merged, 1, 4));
    check_position_near(position_for_local_vertex(merged, 0, 4), center);
    check_position_near(position_for_local_vertex(merged, 1, 4), center);
}

TEST_CASE(
    "merge_clusterings averages positions of welded vertices when average_positions is true",
    "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(
        MergeMode::GreedyLocal,
        MergeMode::ConnectedComponents,
        MergeMode::MultipartiteNearest);

    const Clustering a = make_triangle_clustering(
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(10.0, 0.0, 0.0),
        glm::dvec3(5.0, 10.0, 0.0));
    const Clustering b = make_triangle_clustering(
        glm::dvec3(0.0, 0.0, 0.5),
        glm::dvec3(110.0, 0.0, 0.0),
        glm::dvec3(105.0, 10.0, 0.0));

    const std::vector<Clustering> clusterings = {a, b};
    const Clustering merged = merge_clusterings(
        clusterings,
        1.0,
        MergeOptions{.mode = merge_mode, .average_positions = true});

    REQUIRE(merged.vertex_count() == 5);
    REQUIRE(global_vertex_index(merged, 0, 0) == global_vertex_index(merged, 1, 0));
    check_position_near(
        position_for_local_vertex(merged, 0, 0),
        glm::dvec3(0.0, 0.0, 0.25));
}

TEST_CASE(
    "merge_clusterings keeps the last contributing position when average_positions is false",
    "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(
        MergeMode::GreedyLocal,
        MergeMode::ConnectedComponents,
        MergeMode::MultipartiteNearest);

    const Clustering a = make_triangle_clustering(
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(10.0, 0.0, 0.0),
        glm::dvec3(5.0, 10.0, 0.0));
    const Clustering b = make_triangle_clustering(
        glm::dvec3(0.0, 0.0, 0.5),
        glm::dvec3(110.0, 0.0, 0.0),
        glm::dvec3(105.0, 10.0, 0.0));

    const std::vector<Clustering> clusterings = {a, b};
    const Clustering merged = merge_clusterings(
        clusterings,
        1.0,
        MergeOptions{.mode = merge_mode, .average_positions = false});

    REQUIRE(merged.vertex_count() == 5);
    REQUIRE(global_vertex_index(merged, 0, 0) == global_vertex_index(merged, 1, 0));
    check_position_near(
        position_for_local_vertex(merged, 0, 0),
        glm::dvec3(0.0, 0.0, 0.5));
}

TEST_CASE(
    "merge_clusterings welds coincident boundary vertices at earth-centered magnitudes",
    "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(
        MergeMode::GreedyLocal,
        MergeMode::ConnectedComponents,
        MergeMode::MultipartiteNearest);

    // Epsilon and position of an actual level 14 node of the stephansdom dataset.
    const double epsilon = 0.25718408203125;
    const glm::dvec3 shared(4085801.6236, 1199335.1025, 4732875.8476);

    const Clustering a = make_triangle_clustering(
        shared,
        shared + glm::dvec3(10.0, 0.0, 0.0),
        shared + glm::dvec3(5.0, 10.0, 0.0));
    const Clustering b = make_triangle_clustering(
        shared,
        shared + glm::dvec3(-10.0, 0.0, 0.0),
        shared + glm::dvec3(-5.0, 10.0, 0.0));

    const std::vector<Clustering> clusterings = {a, b};
    const Clustering merged = merge_clusterings(
        clusterings,
        epsilon,
        MergeOptions{.mode = merge_mode});

    REQUIRE(merged.clusters.size() == 2);
    CHECK(merged.vertex_count() == 5);
    CHECK(global_vertex_index(merged, 0, 0) == global_vertex_index(merged, 1, 0));
}

namespace {

// Two separate triangles in one clustering, sharing a position but not a vertex index.
// The pair at the origin is a crack that only interior merging is allowed to close.
Clustering make_cracked_clustering() {
    Clustering clustering;
    clustering.positions = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(10.0, 0.0, 0.0),
        glm::dvec3(5.0, 10.0, 0.0),
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(-10.0, 0.0, 0.0),
        glm::dvec3(-5.0, 10.0, 0.0),
    };

    for (uint32_t cluster_index = 0; cluster_index < 2; cluster_index++) {
        const uint32_t offset = cluster_index * 3;

        Cluster cluster;
        cluster.id = cluster_index;
        cluster.vertex_indices = {offset, offset + 1, offset + 2};
        cluster.local_triangles = {glm::uvec3(0, 1, 2)};
        clustering.clusters.push_back(std::move(cluster));
    }

    return clustering;
}

// A triangle far enough away that it never welds with make_cracked_clustering.
Clustering make_distant_clustering() {
    return make_triangle_clustering(
        glm::dvec3(100.0, 0.0, 0.0),
        glm::dvec3(110.0, 0.0, 0.0),
        glm::dvec3(105.0, 10.0, 0.0));
}
} // namespace

TEST_CASE(
    "merge_clusterings keeps coincident vertices of one clustering apart by default",
    "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(
        MergeMode::GreedyLocal,
        MergeMode::ConnectedComponents,
        MergeMode::MultipartiteNearest);

    const std::vector<Clustering> clusterings = {make_cracked_clustering(), make_distant_clustering()};
    const Clustering merged = merge_clusterings(
        clusterings,
        1.0,
        MergeOptions{.mode = merge_mode});

    REQUIRE(merged.clusters.size() == 3);
    CHECK(merged.vertex_count() == 9);
    CHECK(global_vertex_index(merged, 0, 0) != global_vertex_index(merged, 1, 0));
}

TEST_CASE(
    "merge_clusterings welds coincident vertices of one clustering when interior merges are allowed",
    "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(
        MergeMode::GreedyLocal,
        MergeMode::ConnectedComponents);

    const std::vector<Clustering> clusterings = {make_cracked_clustering(), make_distant_clustering()};
    const Clustering merged = merge_clusterings(
        clusterings,
        1.0,
        MergeOptions{.mode = merge_mode, .allow_interior_merges = true});

    REQUIRE(merged.clusters.size() == 3);
    CHECK(merged.vertex_count() == 8);
    CHECK(global_vertex_index(merged, 0, 0) == global_vertex_index(merged, 1, 0));
    check_position_near(position_for_local_vertex(merged, 0, 0), glm::dvec3(0.0, 0.0, 0.0));
}

TEST_CASE(
    "merge_clusterings falls back to connected components for interior merges",
    "[dag_builder][merge]") {
    const std::vector<Clustering> clusterings = {make_cracked_clustering(), make_distant_clustering()};
    const Clustering merged = merge_clusterings(
        clusterings,
        1.0,
        MergeOptions{.mode = MergeMode::MultipartiteNearest, .allow_interior_merges = true});

    REQUIRE(merged.clusters.size() == 3);
    CHECK(merged.vertex_count() == 8);
    CHECK(global_vertex_index(merged, 0, 0) == global_vertex_index(merged, 1, 0));
}

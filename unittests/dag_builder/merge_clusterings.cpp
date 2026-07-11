#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <opencv2/opencv.hpp>
#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "cluster.h"
#include "merge/clusterings.h"

namespace {

cv::Mat make_texture(const uchar tag) {
    cv::Mat texture(1, 1, CV_8UC3);
    texture.at<cv::Vec3b>(0, 0) = cv::Vec3b(tag, tag, tag);
    return texture;
}

// A single, unconnected triangle. Since none of its edges have a twin, all
// three vertices are boundary vertices and are eligible for welding.
Clustering make_triangle_clustering(const glm::dvec3 &v0, const glm::dvec3 &v1, const glm::dvec3 &v2, const uchar texture_tag = 0) {
    Clustering clustering;
    clustering.positions = {v0, v1, v2};
    clustering.textures.add(make_texture(texture_tag));

    Cluster cluster;
    cluster.id = 0;
    cluster.vertex_indices = {0, 1, 2};
    cluster.local_triangles = {glm::uvec3(0, 1, 2)};
    cluster.texture_id = 0;
    clustering.clusters.push_back(std::move(cluster));

    return clustering;
}

// A closed fan of four triangles around a center vertex. Every edge touching
// the center vertex is shared by two triangles (and thus has a twin), so the
// center vertex is interior. The four outer vertices each touch one of the
// unmatched outer boundary edges, so they remain boundary vertices.
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
    cluster.texture_id = 0;
    clustering.clusters.push_back(std::move(cluster));

    return clustering;
}

bool contains_position_near(const std::vector<glm::dvec3> &positions, const glm::dvec3 &target, const double tolerance) {
    for (const glm::dvec3 &position : positions) {
        if (glm::distance(position, target) <= tolerance) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("merge_clusterings returns an empty clustering for empty input", "[dag_builder][merge]") {
    const std::vector<Clustering> clusterings;

    const Clustering merged = merge_clusterings(clusterings, 0.01);

    CHECK(merged.positions.empty());
    CHECK(merged.clusters.empty());
}

TEST_CASE("merge_clusterings returns a copy of the single input clustering unchanged", "[dag_builder][merge]") {
    const Clustering clustering = make_triangle_clustering(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(1.0, 0.0, 0.0), glm::dvec3(0.0, 1.0, 0.0));
    const std::vector<Clustering> clusterings = {clustering};

    const Clustering merged = merge_clusterings(clusterings, 0.01);

    REQUIRE(merged.positions.size() == clustering.positions.size());
    for (size_t i = 0; i < clustering.positions.size(); i++) {
        CHECK(merged.positions[i] == clustering.positions[i]);
    }
    REQUIRE(merged.clusters.size() == clustering.clusters.size());
    CHECK(merged.clusters[0].vertex_indices == clustering.clusters[0].vertex_indices);
    CHECK(merged.clusters[0].local_triangles == clustering.clusters[0].local_triangles);
}

TEST_CASE("merge_clusterings throws std::invalid_argument for non-positive or non-finite epsilon", "[dag_builder][merge]") {
    const std::vector<Clustering> clusterings = {
        make_triangle_clustering(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0)),
        make_triangle_clustering(glm::dvec3(0.001, 0.0, 0.0), glm::dvec3(110.0, 0.0, 0.0), glm::dvec3(105.0, 10.0, 0.0)),
    };

    CHECK_THROWS_AS(merge_clusterings(clusterings, 0.0), std::invalid_argument);
    CHECK_THROWS_AS(merge_clusterings(clusterings, -1.0), std::invalid_argument);
    CHECK_THROWS_AS(merge_clusterings(clusterings, std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
    CHECK_THROWS_AS(merge_clusterings(clusterings, std::numeric_limits<double>::infinity()), std::invalid_argument);
}

TEST_CASE("merge_clusterings welds boundary vertices within epsilon and preserves triangles", "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(MergeMode::GreedyLocal, MergeMode::ConnectedComponents, MergeMode::MultipartiteNearest);

    const Clustering a = make_triangle_clustering(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0));
    const Clustering b = make_triangle_clustering(glm::dvec3(0.001, 0.0, 0.0), glm::dvec3(110.0, 0.0, 0.0), glm::dvec3(105.0, 10.0, 0.0));
    const std::vector<Clustering> clusterings = {a, b};

    const Clustering merged = merge_clusterings(clusterings, 0.01, merge_mode);

    REQUIRE(merged.clusters.size() == 2);
    CHECK(merged.clusters[0].local_triangles.size() == 1);
    CHECK(merged.clusters[1].local_triangles.size() == 1);
    REQUIRE(merged.vertex_count() == 5);
    CHECK(contains_position_near(merged.positions, glm::dvec3(0.0005, 0.0, 0.0), 1e-9));
    CHECK(contains_position_near(merged.positions, glm::dvec3(10.0, 0.0, 0.0), 1e-9));
    CHECK(contains_position_near(merged.positions, glm::dvec3(110.0, 0.0, 0.0), 1e-9));
}

TEST_CASE("merge_clusterings does not weld vertices further apart than epsilon", "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(MergeMode::GreedyLocal, MergeMode::ConnectedComponents, MergeMode::MultipartiteNearest);

    const Clustering a = make_triangle_clustering(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0));
    const Clustering b = make_triangle_clustering(glm::dvec3(1.0, 0.0, 0.0), glm::dvec3(110.0, 0.0, 0.0), glm::dvec3(105.0, 10.0, 0.0));
    const std::vector<Clustering> clusterings = {a, b};

    const Clustering merged = merge_clusterings(clusterings, 0.01, merge_mode);

    CHECK(merged.vertex_count() == 6);
}

TEST_CASE("merge_clusterings welds mutually close boundary vertices across three clusterings", "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(MergeMode::GreedyLocal, MergeMode::ConnectedComponents, MergeMode::MultipartiteNearest);

    const Clustering a = make_triangle_clustering(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0));
    const Clustering b = make_triangle_clustering(glm::dvec3(0.001, 0.0, 0.0), glm::dvec3(110.0, 0.0, 0.0), glm::dvec3(105.0, 10.0, 0.0));
    const Clustering c = make_triangle_clustering(glm::dvec3(-0.001, 0.0, 0.0), glm::dvec3(210.0, 0.0, 0.0), glm::dvec3(205.0, 10.0, 0.0));
    const std::vector<Clustering> clusterings = {a, b, c};

    const Clustering merged = merge_clusterings(clusterings, 0.01, merge_mode);

    REQUIRE(merged.clusters.size() == 3);
    CHECK(merged.clusters[0].local_triangles.size() == 1);
    CHECK(merged.clusters[1].local_triangles.size() == 1);
    CHECK(merged.clusters[2].local_triangles.size() == 1);
    REQUIRE(merged.vertex_count() == 7);
}

TEST_CASE("merge_clusterings averages the position of a vertex welded across three clusterings", "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(MergeMode::GreedyLocal, MergeMode::ConnectedComponents, MergeMode::MultipartiteNearest);

    const Clustering a = make_triangle_clustering(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0));
    const Clustering b = make_triangle_clustering(glm::dvec3(0.001, 0.0, 0.0), glm::dvec3(110.0, 0.0, 0.0), glm::dvec3(105.0, 10.0, 0.0));
    const Clustering c = make_triangle_clustering(glm::dvec3(-0.001, 0.0, 0.0), glm::dvec3(210.0, 0.0, 0.0), glm::dvec3(205.0, 10.0, 0.0));
    const std::vector<Clustering> clusterings = {a, b, c};

    const Clustering merged = merge_clusterings(clusterings, 0.01, merge_mode, true);

    REQUIRE(merged.vertex_count() == 7);
    CHECK(contains_position_near(merged.positions, glm::dvec3(0.0, 0.0, 0.0), 1e-9));
}

TEST_CASE("merge_clusterings never welds interior vertices even when coincident", "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(MergeMode::GreedyLocal, MergeMode::ConnectedComponents, MergeMode::MultipartiteNearest);

    const glm::dvec3 center(0.0, 0.0, 0.0);
    const Clustering a = make_fan_clustering(center, glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(0.0, 10.0, 0.0), glm::dvec3(-10.0, 0.0, 0.0), glm::dvec3(0.0, -10.0, 0.0));
    const Clustering b = make_fan_clustering(center, glm::dvec3(510.0, 0.0, 0.0), glm::dvec3(500.0, 10.0, 0.0), glm::dvec3(490.0, 0.0, 0.0), glm::dvec3(500.0, -10.0, 0.0));
    const std::vector<Clustering> clusterings = {a, b};

    const Clustering merged = merge_clusterings(clusterings, 0.01, merge_mode);

    CHECK(merged.vertex_count() == 10);
}

TEST_CASE("merge_clusterings averages positions of welded vertices when average_positions is true", "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(MergeMode::GreedyLocal, MergeMode::ConnectedComponents, MergeMode::MultipartiteNearest);

    const Clustering a = make_triangle_clustering(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0));
    const Clustering b = make_triangle_clustering(glm::dvec3(0.0, 0.0, 0.5), glm::dvec3(110.0, 0.0, 0.0), glm::dvec3(105.0, 10.0, 0.0));
    const std::vector<Clustering> clusterings = {a, b};

    const Clustering merged = merge_clusterings(clusterings, 1.0, merge_mode, true);

    REQUIRE(merged.vertex_count() == 5);
    CHECK(contains_position_near(merged.positions, glm::dvec3(0.0, 0.0, 0.25), 1e-9));
}

TEST_CASE("merge_clusterings keeps the last contributing position when average_positions is false", "[dag_builder][merge]") {
    const MergeMode merge_mode = GENERATE(MergeMode::GreedyLocal, MergeMode::ConnectedComponents, MergeMode::MultipartiteNearest);

    const Clustering a = make_triangle_clustering(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0));
    const Clustering b = make_triangle_clustering(glm::dvec3(0.0, 0.0, 0.5), glm::dvec3(110.0, 0.0, 0.0), glm::dvec3(105.0, 10.0, 0.0));
    const std::vector<Clustering> clusterings = {a, b};

    const Clustering merged = merge_clusterings(clusterings, 1.0, merge_mode, false);

    REQUIRE(merged.vertex_count() == 5);
    CHECK(contains_position_near(merged.positions, glm::dvec3(0.0, 0.0, 0.5), 1e-9));
    CHECK_FALSE(contains_position_near(merged.positions, glm::dvec3(0.0, 0.0, 0.0), 1e-9));
}

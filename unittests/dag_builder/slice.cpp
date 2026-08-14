#include <cstdint>
#include <vector>

#include <opencv2/opencv.hpp>
#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "cluster.h"
#include "slice.h"

namespace {

cv::Mat make_texture(const uchar tag) {
    cv::Mat texture(1, 1, CV_8UC3);
    texture.at<cv::Vec3b>(0, 0) = cv::Vec3b(tag, tag, tag);
    return texture;
}

Clustering make_clustering(const uint32_t vertex_count, const uint32_t texture_count) {
    Clustering clustering;
    for (uint32_t i = 0; i < vertex_count; i++) {
        clustering.positions.push_back(glm::dvec3(i, 0.0, 0.0));
    }
    for (uint32_t i = 0; i < texture_count; i++) {
        clustering.textures.add(make_texture(i));
    }
    return clustering;
}

} // namespace

TEST_CASE("slice_clusters keeps only the requested clusters and compacts vertices", "[dag_builder][slice]") {
    Clustering clustering = make_clustering(4, 1);
    for (uint32_t i = 0; i < 4; i++) {
        Cluster cluster;
        cluster.vertex_indices = {i};
        cluster.texture_id = 0;
        clustering.clusters.push_back(std::move(cluster));
    }

    const Clustering sliced = slice_clusters(clustering, std::vector<uint32_t>{1, 3});

    REQUIRE(sliced.clusters.size() == 2);
    REQUIRE(sliced.positions.size() == 2);
    CHECK(sliced.clusters[0].vertex_indices == std::vector<uint32_t>{0});
    CHECK(sliced.clusters[1].vertex_indices == std::vector<uint32_t>{1});
    CHECK(sliced.positions[0] == clustering.positions[1]);
    CHECK(sliced.positions[1] == clustering.positions[3]);
}

TEST_CASE("slice_clusters remaps texture ids", "[dag_builder][slice]") {
    Clustering clustering = make_clustering(4, 4);
    for (uint32_t i = 0; i < 4; i++) {
        Cluster cluster;
        cluster.vertex_indices = {i};
        cluster.texture_id = i;
        clustering.clusters.push_back(std::move(cluster));
    }

    const Clustering sliced = slice_clusters(clustering, std::vector<uint32_t>{3});

    REQUIRE(sliced.clusters.size() == 1);
    REQUIRE(sliced.textures.size() == 1);

    REQUIRE(sliced.clusters[0].is_textured());
    const uint32_t new_texture_id = sliced.clusters[0].texture_id.value();
    REQUIRE(new_texture_id < sliced.textures.size());
    CHECK(sliced.textures[new_texture_id].data == clustering.textures[3].data);
}

TEST_CASE("slice_clusters preserves per-cluster fields", "[dag_builder][slice]") {
    Clustering clustering = make_clustering(3, 1);

    Cluster cluster;
    cluster.id = 42;
    cluster.vertex_indices = {0, 1, 2};
    cluster.local_triangles = {glm::uvec3(0, 1, 2)};
    cluster.uvs = {glm::dvec2(0.0, 0.0), glm::dvec2(1.0, 0.0), glm::dvec2(0.0, 1.0)};
    cluster.texture_id = 0;
    cluster.absolute_error = 1.5;
    clustering.clusters.push_back(cluster);

    const Clustering sliced = slice_clusters(clustering, std::vector<uint32_t>{0});

    REQUIRE(sliced.clusters.size() == 1);
    const Cluster &result = sliced.clusters[0];
    CHECK(result.id == cluster.id);
    CHECK(result.local_triangles == cluster.local_triangles);
    CHECK(result.uvs == cluster.uvs);
    CHECK(result.absolute_error == cluster.absolute_error);
}

TEST_CASE("slice_clusters returns the input unchanged when all clusters are selected", "[dag_builder][slice]") {
    Clustering clustering = make_clustering(2, 1);
    for (uint32_t i = 0; i < 2; i++) {
        Cluster cluster;
        cluster.vertex_indices = {i};
        cluster.texture_id = 0;
        clustering.clusters.push_back(std::move(cluster));
    }

    const Clustering sliced = slice_clusters(clustering, std::vector<uint32_t>{0, 1});

    REQUIRE(sliced.clusters.size() == clustering.clusters.size());
    REQUIRE(sliced.positions.size() == clustering.positions.size());
    REQUIRE(sliced.textures.size() == clustering.textures.size());
}

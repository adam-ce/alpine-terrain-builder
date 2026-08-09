#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "../catch2_helpers.h"

#include "atlas/bake_cluster_texture.h"
#include "range_utils.h"

namespace {

constexpr glm::uvec2 texture_size(4, 4);

// A flat unit quad, split into two triangles over four shared vertices.
Clustering make_quad_clustering(const cv::Mat &texture) {
    Cluster cluster;
    cluster.vertex_indices = {0, 1, 2, 3};
    cluster.local_triangles = {{0, 1, 2}, {0, 2, 3}};
    cluster.uvs = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    cluster.texture_id = 0;

    Clustering clustering;
    clustering.positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    clustering.clusters = {std::move(cluster)};
    clustering.textures.add(texture);
    return clustering;
}

// Each texel a different colour, so any mix-up shows up as a wrong pixel rather than a wrong shade.
cv::Mat make_distinct_texture() {
    cv::Mat texture(texture_size.y, texture_size.x, CV_8UC3);
    for (const uint32_t y : range<uint32_t>(texture_size.y)) {
        for (const uint32_t x : range<uint32_t>(texture_size.x)) {
            texture.at<cv::Vec3b>(y, x) = cv::Vec3b(uint8_t(10 * x), uint8_t(10 * y), 200);
        }
    }
    return texture;
}

BakeTextureOptions nearest_options() {
    BakeTextureOptions options;
    options.reprojection = ReprojectionOptions{1, cv::INTER_NEAREST, 0};
    return options;
}

} // namespace

TEST_CASE("bake_cluster_texture carries a mirrored layout through", "[dagbuilder][bake_cluster]") {
    const Clustering clustering = make_quad_clustering(make_distinct_texture());
    const std::vector<uint32_t> sources = {0};
    const BakeSource source = collect_bake_source(clustering, sources);

    // The target lays the same surface out mirrored in x, so an identity bake cannot pass.
    Cluster target = clustering.clusters[0];
    target.uvs = {{1.0, 0.0}, {0.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}};

    const cv::Mat baked = bake_cluster_texture(target, clustering.positions, source, texture_size, nearest_options());

    const cv::Mat expected = make_distinct_texture();
    for (const uint32_t y : range<uint32_t>(texture_size.y)) {
        for (const uint32_t x : range<uint32_t>(texture_size.x)) {
            REQUIRE(baked.at<cv::Vec3b>(y, x) == expected.at<cv::Vec3b>(y, texture_size.x - 1 - x));
        }
    }
}

TEST_CASE("bake_cluster_texture reads each half from its own source", "[dagbuilder][bake_cluster]") {
    const cv::Vec3b left_colour(200, 0, 0);
    const cv::Vec3b right_colour(0, 200, 0);

    // Two clusters, each a solid colour, splitting the quad down the middle.
    Cluster left;
    left.vertex_indices = {0, 1, 2, 3};
    left.local_triangles = {{0, 1, 2}, {0, 2, 3}};
    left.uvs = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    left.texture_id = 0;

    Cluster right = left;
    right.vertex_indices = {1, 4, 5, 2};
    right.texture_id = 1;

    Clustering clustering;
    clustering.positions = {{0, 0, 0}, {0.5, 0, 0}, {0.5, 1, 0}, {0, 1, 0}, {1, 0, 0}, {1, 1, 0}};
    clustering.clusters = {std::move(left), std::move(right)};
    clustering.textures.add(cv::Mat(texture_size.y, texture_size.x, CV_8UC3, left_colour));
    clustering.textures.add(cv::Mat(texture_size.y, texture_size.x, CV_8UC3, right_colour));

    const std::vector<uint32_t> sources = {0, 1};
    const BakeSource source = collect_bake_source(clustering, sources);

    // One target cluster spanning both halves.
    Cluster target;
    target.vertex_indices = {0, 4, 5, 3};
    target.local_triangles = {{0, 1, 2}, {0, 2, 3}};
    target.uvs = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};

    const cv::Mat baked = bake_cluster_texture(target, clustering.positions, source, texture_size, nearest_options());

    for (const uint32_t y : range<uint32_t>(texture_size.y)) {
        REQUIRE(baked.at<cv::Vec3b>(y, 0) == left_colour);
        REQUIRE(baked.at<cv::Vec3b>(y, texture_size.x - 1) == right_colour);
    }
}
// A node atlas lays out every cluster's triangles in one uv space, in the order they were
// handed to the unwrap, and duplicates a vertex wherever a chart seam runs through it.
TEST_CASE("apply_node_atlas splits a shared atlas back into its clusters", "[dag_builder][atlas]") {
    Clustering clustering;
    clustering.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}};

    Cluster first;
    first.vertex_indices = {0, 1, 2};
    first.local_triangles = {{0, 1, 2}};

    Cluster second;
    second.vertex_indices = {1, 2, 3};
    second.local_triangles = {{0, 1, 2}};

    clustering.clusters = {first, second};

    // Global vertex 1 sits on a seam, so it arrives as atlas vertices 1 and 4.
    uv::Atlas atlas;
    atlas.vertex_map = {0, 1, 2, 3, 1};
    atlas.triangles = {{0, 1, 2}, {4, 2, 3}};
    atlas.uvs = {{0.0, 0.0}, {0.5, 0.0}, {0.0, 0.5}, {0.5, 0.5}, {1.0, 0.0}};
    atlas.size = {8, 4};

    apply_node_atlas(clustering, std::vector<uint32_t>{0, 1}, atlas);

    CHECK(clustering.clusters[0].vertex_indices == std::vector<uint32_t>{0, 1, 2});
    CHECK(clustering.clusters[0].local_triangles == std::vector<glm::uvec3>{{0, 1, 2}});
    CHECK(clustering.clusters[0].uvs == std::vector<glm::dvec2>{{0.0, 0.0}, {0.5, 0.0}, {0.0, 0.5}});

    // The second cluster indexes its own vertices from zero, and takes the seam duplicate's uv.
    CHECK(clustering.clusters[1].vertex_indices == std::vector<uint32_t>{1, 2, 3});
    CHECK(clustering.clusters[1].local_triangles == std::vector<glm::uvec3>{{0, 1, 2}});
    CHECK(clustering.clusters[1].uvs == std::vector<glm::dvec2>{{1.0, 0.0}, {0.0, 0.5}, {0.5, 0.5}});
}

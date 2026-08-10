#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "../catch2_helpers.h"

#include "cluster.h"
#include "texture_reuse.h"

namespace {

// Two triangles of a quad, meeting along the shared vertices 1 and 2.
Clustering make_quad_clustering(const std::optional<uint32_t> texture_id) {
    Clustering clustering;
    clustering.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}};
    clustering.textures.add(cv::Mat::zeros(4, 4, CV_8UC3));

    Cluster first;
    first.vertex_indices = {0, 1, 2};
    first.local_triangles = {{0, 1, 2}};
    first.uvs = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}};
    first.texture_id = texture_id;

    Cluster second;
    second.vertex_indices = {1, 2, 3};
    second.local_triangles = {{0, 1, 2}};
    second.uvs = {{1.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}};
    second.texture_id = texture_id;

    clustering.clusters = {first, second};
    return clustering;
}

// The cluster the two source triangles merge into.
Cluster make_merged_cluster() {
    Cluster merged;
    merged.vertex_indices = {0, 1, 2, 3};
    merged.local_triangles = {{0, 1, 2}, {1, 2, 3}};
    return merged;
}

const std::vector<uint32_t> both_clusters{0, 1};

} // namespace

TEST_CASE("inherit_shared_texture carries a texture whose uvs agree at the seam", "[dag_builder][texture_reuse]") {
    const Clustering source = make_quad_clustering(0);
    Cluster merged = make_merged_cluster();
    TextureSet textures;

    inherit_shared_texture(merged, textures, source, both_clusters);

    // The texture is copied into the merged set, which had none of its own.
    REQUIRE(textures.size() == 1);
    REQUIRE(merged.texture_id == 0);
    CHECK(merged.uvs == std::vector<glm::dvec2>{{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}});
}

// A chart seam gives one vertex two uvs, which the merged cluster has no way to represent.
TEST_CASE("inherit_shared_texture refuses a texture whose uvs disagree at the seam", "[dag_builder][texture_reuse]") {
    Clustering source = make_quad_clustering(0);
    source.clusters[1].uvs[0] = {0.5, 0.5};
    Cluster merged = make_merged_cluster();
    TextureSet textures;

    inherit_shared_texture(merged, textures, source, both_clusters);

    CHECK_FALSE(merged.is_textured());
    CHECK(merged.uvs.empty());
}

TEST_CASE("inherit_shared_texture refuses when the sources carry different textures", "[dag_builder][texture_reuse]") {
    Clustering source = make_quad_clustering(0);
    source.textures.add(cv::Mat::ones(4, 4, CV_8UC3));
    source.clusters[1].texture_id = 1;
    Cluster merged = make_merged_cluster();
    TextureSet textures;

    inherit_shared_texture(merged, textures, source, both_clusters);

    CHECK_FALSE(merged.is_textured());
}

TEST_CASE("inherit_shared_texture leaves untextured sources alone", "[dag_builder][texture_reuse]") {
    const Clustering source = make_quad_clustering(std::nullopt);
    Cluster merged = make_merged_cluster();
    TextureSet textures;

    inherit_shared_texture(merged, textures, source, both_clusters);

    CHECK_FALSE(merged.is_textured());
    CHECK(merged.uvs.empty());
}

#include "../catch2_helpers.h"

#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "cluster.h"
#include "merge/weld.h"

namespace {

constexpr double position_tolerance = 1e-9;

Cluster make_cluster(std::vector<uint32_t> vertex_indices, std::vector<glm::uvec3> local_triangles) {
    Cluster cluster;
    cluster.id = 0;
    cluster.vertex_indices = std::move(vertex_indices);
    cluster.local_triangles = std::move(local_triangles);
    return cluster;
}

// Two triangles sharing a position but not a vertex index: a crack inside a single clustering.
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
    clustering.clusters.push_back(make_cluster({0, 1, 2}, {glm::uvec3(0, 1, 2)}));
    clustering.clusters.push_back(make_cluster({3, 4, 5}, {glm::uvec3(0, 1, 2)}));

    return clustering;
}

uint32_t global_vertex_index(
    const Clustering &clustering,
    const uint32_t cluster_index,
    const uint32_t local_vertex_index) {
    return clustering.clusters.at(cluster_index).vertex_indices.at(local_vertex_index);
}
} // namespace

TEST_CASE("weld_clustering closes a crack inside one clustering", "[dag_builder][weld]") {
    const Clustering welded = weld_clustering(make_cracked_clustering(), 1.0);

    REQUIRE(welded.clusters.size() == 2);
    CHECK(welded.vertex_count() == 5);
    CHECK(global_vertex_index(welded, 0, 0) == global_vertex_index(welded, 1, 0));
}

TEST_CASE("weld_clustering leaves distinct vertices apart", "[dag_builder][weld]") {
    Clustering clustering = make_cracked_clustering();
    clustering.positions[3] = glm::dvec3(0.0, -5.0, 0.0);

    const Clustering welded = weld_clustering(clustering, 1.0);

    CHECK(welded.vertex_count() == 6);
    CHECK(global_vertex_index(welded, 0, 0) != global_vertex_index(welded, 1, 0));
}

TEST_CASE("weld_clustering places the welded vertex by averaging or last writer", "[dag_builder][weld]") {
    const bool average_positions = GENERATE(true, false);

    Clustering clustering = make_cracked_clustering();
    clustering.positions[3] = glm::dvec3(0.4, 0.0, 0.0);

    const Clustering welded = weld_clustering(clustering, 1.0, average_positions);

    REQUIRE(welded.vertex_count() == 5);
    const glm::dvec3 &position = welded.positions[global_vertex_index(welded, 0, 0)];
    const glm::dvec3 expected = average_positions ? glm::dvec3(0.2, 0.0, 0.0) : glm::dvec3(0.4, 0.0, 0.0);
    CHECK(glm::distance(position, expected) <= position_tolerance);
}

TEST_CASE("weld_clustering drops triangles collapsed by welding", "[dag_builder][weld]") {
    Clustering clustering;
    clustering.positions = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(10.0, 0.0, 0.0),
        glm::dvec3(5.0, 10.0, 0.0),
        glm::dvec3(0.1, 0.0, 0.0),
    };
    clustering.clusters.push_back(make_cluster({0, 1, 2, 3}, {glm::uvec3(0, 1, 2), glm::uvec3(0, 3, 1)}));

    const Clustering welded = weld_clustering(clustering, 1.0);

    REQUIRE(welded.clusters.size() == 1);
    CHECK(welded.vertex_count() == 3);
    CHECK(welded.clusters[0].local_triangles.size() == 1);
    CHECK(welded.clusters[0].vertex_indices.size() == 3);
}

TEST_CASE("weld_clustering rejects a non positive epsilon", "[dag_builder][weld]") {
    const Clustering clustering = make_cracked_clustering();

    CHECK_THROWS_AS(weld_clustering(clustering, 0.0), std::invalid_argument);
    CHECK_THROWS_AS(weld_clustering(clustering, -1.0), std::invalid_argument);
}

TEST_CASE("weld_clustering returns an empty clustering unchanged", "[dag_builder][weld]") {
    const Clustering welded = weld_clustering(Clustering{}, 1.0);

    CHECK(welded.is_empty());
}
#include <filesystem>
#include <queue>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

#include "cluster.h"
#include "clusterize.h"
#include "mesh/io.h"
#include "partition.h"
#include "mesh/bounds.h"
#include "simplify.h"
#include "split.h"
#include "utils.h"
#include "validate.h"
#include "uv.h"
#include "atlas/atlas.h"
#include "log.h"

constexpr double EARTH_CIRCUMFERENCE_M = 40075016.686; // Earth's circumference in metres
constexpr int MAX_LEVEL = 21;                          // adjust as needed (0..20 for standard OSM zooms)

// Compute metres per pixel at equator for a given zoom level
constexpr double metres_per_pixel(int zoom) {
    return EARTH_CIRCUMFERENCE_M / (256.0 * (1ULL << zoom));
}

// Fill a constexpr std::array automatically
constexpr std::array<double, MAX_LEVEL> generate_meters_per_pixel_array() {
    std::array<double, MAX_LEVEL> arr = {};
    for (int z = 0; z < MAX_LEVEL; ++z) {
        arr[z] = metres_per_pixel(z);
    }
    return arr;
}

constexpr std::array<double, MAX_LEVEL> METERS_PER_PIXEL_AT_EQUATOR = generate_meters_per_pixel_array();

// Test values (compare with known table values)
static_assert(std::abs(METERS_PER_PIXEL_AT_EQUATOR[0] - 156543) < 1, "level 0 mismatch");
static_assert(std::abs(METERS_PER_PIXEL_AT_EQUATOR[10] - 152.874) < 0.001, "level 10 mismatch");
static_assert(std::abs(METERS_PER_PIXEL_AT_EQUATOR[20] - 0.149) < 0.001, "level 20 mismatch");


inline radix::geometry::Aabb3d calculate_bounds(const Clustering& clustering) {
    return mesh::calculate_bounds(clustering.positions);
}

inline std::vector<uint8_t> find_vertices_to_lock(const Clustering& clustering) {
    // Allocate vertex lock buffer
    const uint32_t vertex_count = clustering.vertex_count();
    std::vector<uint8_t> vertex_lock(vertex_count, VertexLock::UNLOCKED);

    // Calculate safe bounds excluding the outer boundary of the mesh
    const radix::geometry::Aabb3d bounds = calculate_bounds(clustering);
    const glm::dvec3 center = bounds.centre();
    const glm::dvec3 extents = bounds.size() / 2.0;
    const glm::dvec3 unlocked_extents = extents * 0.99;
    const radix::geometry::Aabb3d unlocked_bounds(center - unlocked_extents, center + unlocked_extents);

    // Lock vertices outside safe bounds
    for (uint32_t vertex_index=0; vertex_index<vertex_count; vertex_index++) {
        const glm::dvec3& position = clustering.positions[vertex_index];
        if (!unlocked_bounds.contains(position)) {
            vertex_lock[vertex_index] = VertexLock::LOCKED;
        }
    }

    // Look for vertices shared between at least 2 clusters and lock them
    constexpr uint32_t invalid_cluster = std::numeric_limits<uint32_t>::max();
    const uint32_t cluster_count = clustering.cluster_count();
    std::vector<uint32_t> cluster_membership(vertex_count, invalid_cluster);
    for (uint32_t cluster_index=0; cluster_index<cluster_count; cluster_index++) {
        const Cluster& cluster = clustering.clusters[cluster_index];
        for (const uint32_t vertex_index : cluster.vertex_indices) {
            uint32_t& other_cluster_index = cluster_membership[vertex_index];
            if (other_cluster_index == invalid_cluster) {
                // This vertex was encountered the first time
                cluster_membership[vertex_index] = cluster_index;
            } else if (other_cluster_index == cluster_index) {
                // This vertex occurs multiple times in the same cluster -> ignore
            } else {
                // This vertex was already encountered in a different cluster -> lock it
                // we dont remark the cluster since we dont care how often the vertex is referenced
                vertex_lock[vertex_index] = VertexLock::LOCKED;
            }
        }
    }

    return vertex_lock;
}

class ClusteringStageLogger {
public:
    explicit ClusteringStageLogger(const Clustering &clustering)
        : _last_clusters(clustering.cluster_count()),
          _last_vertices(clustering.vertex_count()) {}

    void log_initial() {
        LOG_INFO(
            "stage=initial clusters={} vertices={}",
            this->_last_clusters,
            this->_last_vertices);
    }

    void log(int level, const char *stage, const Clustering &clustering) {
        const uint32_t clusters = clustering.cluster_count();
        const uint32_t vertices = clustering.vertex_count();

        LOG_INFO(
            "level={} stage={} clusters={}->{} vertices={}->{}",
            level,
            stage,
            this->_last_clusters,
            clusters,
            this->_last_vertices,
            vertices);

        this->_last_clusters = clusters;
        this->_last_vertices = vertices;
    }
private:
    uint32_t _last_clusters;
    uint32_t _last_vertices;
};

int main(int argc, char **argv) {
    USE(argc);
    USE(argv);

    const std::filesystem::path input =
        // "/home/user/master/meshes/innenstadt8/13/6478/4796/6856.glb";
        // "/home/user/master/meshes/innenstadt10/15/25917/19184/27423.glb";
        // "/home/user/master/meshes/innenstadt10/15/25914/19185/27426.glb";
        "/home/user/master/meshes/innenstadt11/15/25917/19185/27422.glb";
    const std::filesystem::path out_dir =
            "/home/user/master/meshes/lod_tree";
    std::filesystem::create_directories(out_dir);

    auto mesh = mesh::io::load_from_path(input).value();

    // Perform initial clustering
    auto clustering = clusterize(mesh);
    validate(clustering);

    ClusteringStageLogger stage_logger(clustering);
    stage_logger.log_initial();

    for (int level = 18; level >= 0; level--) {
        // Partition clusters into groups
        clustering = partition(clustering, PartitionOptions{.clusters_per_partition = 8});
        validate(clustering);
        stage_logger.log(level, "partitioned", clustering);

        // Find vertices to lock
        const std::vector<uint8_t> vertex_lock = find_vertices_to_lock(clustering);

        // Simplify each cluster
        clustering = simplify(clustering, SimplifyOptions{
                                              .target_ratio = 0.25,
                                              .vertex_lock = VertexLock::mask(vertex_lock)});
        validate(clustering);
        remove_unused_vertices_inplace(clustering);
        validate(clustering);
        stage_logger.log(level, "simplified", clustering);

        // Output to file
        mesh::io::save_to_path(
            clustering_to_mesh(clustering),
            (out_dir / ("level_" + std::to_string(level) + ".glb")).string());
        if (clustering.clusters.size() == 1) {
            LOG_INFO("Stopping at single remaining cluster");
            break;
        }

        // Split each cluster into roughly 4 parts
        clustering = clusterize(clustering).clustering;
        validate(clustering);
        stage_logger.log(level, "reclustered", clustering);
    }

    return 0;
}

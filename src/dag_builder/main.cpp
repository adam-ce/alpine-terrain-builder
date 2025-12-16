#include <filesystem>

#include "cluster.h"
#include "clusterize.h"
#include "group.h"
#include "mesh/io.h"
#include "simplify.h"
#include "split.h"
#include "utils.h"
#include "validate.h"

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>

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

// The array
constexpr std::array<double, MAX_LEVEL> METERS_PER_PIXEL_AT_EQUATOR = generate_meters_per_pixel_array();

// Test values (compare with known table values)
static_assert(std::abs(METERS_PER_PIXEL_AT_EQUATOR[0] - 156543) < 1, "level 0 mismatch");
static_assert(std::abs(METERS_PER_PIXEL_AT_EQUATOR[10] - 152.874) < 0.001, "level 10 mismatch");
static_assert(std::abs(METERS_PER_PIXEL_AT_EQUATOR[20] - 0.149) < 0.001, "level 20 mismatch");

int main(int argc, char **argv) {
    for (int z = 0; z < MAX_LEVEL; ++z) {
        std::cout << "Zoom " << z << ": " << METERS_PER_PIXEL_AT_EQUATOR[z] << " m/pixel\n";
    }

    const std::filesystem::path path = "/home/user/master/meshes/innenstadt3/13/6478/4795/6857.glb";
    auto mesh = mesh::io::load_from_path(path).value();

    auto clustering = clusterize(mesh);
    validate(clustering);
    const auto clusters_mesh = clustering_to_mesh(clustering);
    mesh::io::save_to_path(clusters_mesh, "/home/user/master/meshes/clusters.glb");

    clustering = group(clustering, GroupOptions{.clusters_per_group = 8});
    validate(clustering);
    const auto grouped_mesh = clustering_to_mesh(clustering);
    mesh::io::save_to_path(grouped_mesh, "/home/user/master/meshes/grouped.glb");

    clustering = simplify(clustering, SimplifyOptions{
                                          .target_ratio = 0,
                                          .absolute_target_error = METERS_PER_PIXEL_AT_EQUATOR[16]});
    validate(clustering);
    clustering.clusters[0] = clustering.clusters[100];
    clustering.clusters.resize(1);
    const auto simplified_mesh = clustering_to_mesh(clustering);
    mesh::io::save_to_path(simplified_mesh, "/home/user/master/meshes/simplified.glb");

    clustering = split_each_into_equal_parts(clustering, 2, 0.95);
    validate(clustering);
    auto c = clustering;
    const auto split_mesh = clustering_to_mesh(c);
    mesh::io::save_to_path(split_mesh, "/home/user/master/meshes/split.glb");
    auto c1 = clustering;
    c1.clusters.resize(1);
    const auto split1_mesh = clustering_to_mesh(c1);
    mesh::io::save_to_path(split1_mesh, "/home/user/master/meshes/split1.glb");
    auto c2 = clustering;
    c2.clusters[0] = c2.clusters[1];
    c2.clusters.resize(1);
    const auto split2_mesh = clustering_to_mesh(c2);
    mesh::io::save_to_path(split2_mesh, "/home/user/master/meshes/split2.glb");
}

/*
void perform(mesh::Simple mesh) {
    const mesh::ComponentsIndex component_index = find_connected_components(mesh);
    Clustering components = clusterize_by_vertex_map(mesh, component_index);

    for (const Cluster component : components.clusters) {
        Clustering clustering = clusterize(component);

        for (size_t iter=0; iter<3; iter++) {
            clustering = group(clustering, GroupOptions{.clusters_per_group = 4});
            for (Cluster &cluster : clustering.cluster) {
                cluster.uv_unwrapping = create_uv_unwrapping(cluster, clustering.positions);
                cluster = simplify(cluster, clustering.positions);
            }
            clustering = split_clusters(clustering);
        }
    }
}

*/

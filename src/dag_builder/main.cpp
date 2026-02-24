#include <filesystem>
#include <queue>

#include "cluster.h"
#include "clusterize.h"
#include "mesh/io.h"
#include "partition.h"
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

constexpr std::array<double, MAX_LEVEL> METERS_PER_PIXEL_AT_EQUATOR = generate_meters_per_pixel_array();

// Test values (compare with known table values)
static_assert(std::abs(METERS_PER_PIXEL_AT_EQUATOR[0] - 156543) < 1, "level 0 mismatch");
static_assert(std::abs(METERS_PER_PIXEL_AT_EQUATOR[10] - 152.874) < 0.001, "level 10 mismatch");
static_assert(std::abs(METERS_PER_PIXEL_AT_EQUATOR[20] - 0.149) < 0.001, "level 20 mismatch");


int main(int argc, char **argv) {
    const std::filesystem::path input =
        "/home/user/master/meshes/innenstadt8/13/6478/4796/6856.glb";
    const std::filesystem::path out_dir =
        "/home/user/master/meshes/lod_tree";
    std::filesystem::create_directories(out_dir);

    auto mesh = mesh::io::load_from_path(input).value();

    auto clustering = clusterize(mesh);
    validate(clustering);

    for (int level = 18; level >= 0; level--) {
        LOG_INFO("Level {}", level);
        clustering = partition(clustering, PartitionOptions{.clusters_per_partition = 8});
        validate(clustering);

        clustering = simplify(clustering, SimplifyOptions{
                                              .target_ratio = 0.5,
                                              .vertex_lock = VertexLock::boundary(),
                                              /*.absolute_target_error = METERS_PER_PIXEL_AT_EQUATOR[level]*/});
        validate(clustering);

        mesh::io::save_to_path(
            clustering_to_mesh(clustering),
            (out_dir / ("level_" + std::to_string(level) + ".glb")).string());

        if (clustering.clusters.size() == 1) {
            break;
        }
        // clustering = split_each_into_equal_parts<2>(clustering);
        clustering = clusterize(clustering).clustering;
        validate(clustering);
    }

    return 0;
}

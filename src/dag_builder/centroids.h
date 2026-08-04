#pragma once

#include <vector>
#include <cstdint>
#include <array>
#include <span>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "cluster.h"
#include "enumerate.h"
#include "octree/Id.h"
#include "octree/Space.h"

inline std::vector<glm::dvec3> compute_cluster_centroids(const Clustering& clustering) {
    std::vector<glm::dvec3> centroids;
    centroids.reserve(clustering.cluster_count());
    for (const Cluster &cluster : clustering.clusters) {
        const uint32_t vertex_count = cluster.vertex_count();
        glm::dvec3 centroid(0);
        for (const uint32_t vertex_index : cluster.vertex_indices) {
            const glm::dvec3 position = clustering.positions[vertex_index];
            centroid += position;
        }
        centroid /= vertex_count;
        centroids.push_back(centroid);
    }
    return centroids;
}

inline std::array<std::vector<uint32_t>, 8> partition_clusters_to_children(const Clustering &clustering, const octree::Id &id, const octree::Space &space = octree::Space::earth()) {
    std::array<std::vector<uint32_t>, 8> result;
    if (!id.has_children()) {
        return result;
    }

    for (std::vector<uint32_t>& clusters : result) {
        clusters.reserve(clustering.cluster_count() / 5);
    }

    const glm::dvec3 centre = space.get_node_bounds(id).centre();
    const auto centroids = compute_cluster_centroids(clustering);
    for (const auto &[cluster_index, centroid] : enumerate(centroids)) {
        const uint8_t x_bit = centroid.x < centre.x ? 0 : 1;
        const uint8_t y_bit = centroid.y < centre.y ? 0 : 1;
        const uint8_t z_bit = centroid.z < centre.z ? 0 : 1;
        const uint8_t child_index = (z_bit << 2) | (y_bit << 1) | x_bit;
        result[child_index].push_back(cluster_index);
    }
    return result;
}


namespace detail {

template <typename Predicate>
inline std::vector<uint32_t> find_clusters_by_centroid_predicate(
    const std::span<const glm::dvec3> centroids,
    Predicate &&predicate) {
    std::vector<uint32_t> clusters;
    clusters.reserve(centroids.size());

    for (const auto &[cluster_index, centroid] : enumerate(centroids)) {
        if (predicate(centroid)) {
            clusters.push_back(static_cast<uint32_t>(cluster_index));
        }
    }

    return clusters;
}

inline bool contained_in_any_bounds(
    const glm::dvec3 &point,
    const std::span<const radix::geometry::Aabb3d> bounds) {
    for (const auto &b : bounds) {
        if (b.contains(point)) {
            return true;
        }
    }

    return false;
}

inline bool contained_in_all_bounds(
    const glm::dvec3 &point,
    const std::span<const radix::geometry::Aabb3d> bounds) {
    for (const auto &b : bounds) {
        if (!b.contains(point)) {
            return false;
        }
    }

    return true;
}

} // namespace detail

struct RegionFilter {
    std::vector<radix::geometry::Aabb3d> include;
    std::vector<radix::geometry::Aabb3d> exclude;

    bool matches(const glm::dvec3 &point) const {
        if (!include.empty()) {
            if (!detail::contained_in_any_bounds(point, include)) {
                return false;
            }
        }
        if (!exclude.empty()) {
            if (detail::contained_in_any_bounds(point, exclude)) {
                return false;
            }
        }
        return true;
    }
};

inline std::vector<uint32_t> find_clusters_inside_bounds(
    const std::span<const glm::dvec3> centroids,
    const radix::geometry::Aabb3d &bounds) {
    return detail::find_clusters_by_centroid_predicate(
        centroids,
        [&bounds](const glm::dvec3 &centroid) {
            return bounds.contains(centroid);
        });
}

inline std::vector<uint32_t> find_clusters_inside_bounds(
    const Clustering &clustering,
    const radix::geometry::Aabb3d &bounds) {
    return find_clusters_inside_bounds(compute_cluster_centroids(clustering), bounds);
}

inline std::vector<uint32_t> find_clusters_matching(
    const std::span<const glm::dvec3> centroids,
    const RegionFilter &filter) {
    return detail::find_clusters_by_centroid_predicate(
        centroids,
        [&filter](const glm::dvec3 &centroid) {
            return filter.matches(centroid);
        });
}

inline std::vector<uint32_t> find_clusters_matching(
    const Clustering &clustering,
    const RegionFilter &filter) {
    return find_clusters_matching(compute_cluster_centroids(clustering), filter);
}

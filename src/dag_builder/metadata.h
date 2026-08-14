#pragma once

#include <cstdint>
#include <vector>

#include <radix/geometry.h>

#include "cluster.h"
#include "dag_id.h"
#include "utils.h"


namespace dag {

struct Group {
    std::vector<Id> children; // higher-detail clusters sharing this group's boundary
    double error; // absolute error compared to the original mesh
    radix::geometry::Aabb3d bounds; // union of the children's bounds
};

struct NodeMetadata {
    std::vector<uint32_t> group_assignment; // cluster index -> group id
    std::vector<Group> groups;

    uint32_t group_count() const {
        return this->groups.size();
    }
};

inline NodeMetadata build_leaf_metadata(const Clustering &clustering) {
    NodeMetadata metadata;
    metadata.group_assignment.reserve(clustering.cluster_count());
    metadata.groups.reserve(clustering.cluster_count());
    for (uint32_t i = 0; i < clustering.cluster_count(); i++) {
        DEBUG_ASSERT(clustering.clusters[i].absolute_error == 0.0);
        metadata.group_assignment.push_back(i);
        Group &group = metadata.groups.emplace_back();
        group.error = 0.0;
        group.bounds = compute_cluster_bounds(clustering.clusters[i], clustering.positions);
    }
    return metadata;
}

// Concatenate metadatas in order.
inline dag::NodeMetadata concat_metadata(std::vector<dag::NodeMetadata> parts) {
    const auto total_clusters = sum(parts, [](const dag::NodeMetadata &part) { return part.group_assignment.size(); });
    const auto total_groups = sum(parts, [](const dag::NodeMetadata &part) { return part.groups.size(); });

    dag::NodeMetadata result;
    result.group_assignment.reserve(total_clusters);
    result.groups.reserve(total_groups);

    for (dag::NodeMetadata &part : parts) {
        const uint32_t group_offset = result.group_count();
        for (const uint32_t group_index : part.group_assignment) {
            result.group_assignment.push_back(group_offset + group_index);
        }
        for (dag::Group &group : part.groups) {
            result.groups.push_back(std::move(group));
        }
    }
    return result;
}

// Get the group bounds based on a cluster index.
inline const radix::geometry::Aabb3d &get_group_bounds(const NodeMetadata &metadata, const uint32_t cluster_index) {
    return metadata.groups[metadata.group_assignment[cluster_index]].bounds;
}

} // namespace dag

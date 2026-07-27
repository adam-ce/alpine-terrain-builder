#pragma once

#include <vector>
#include <optional>

#include "octree/Id.h"
#include "cluster.h"
#include "enumerate.h"

namespace dag {

struct Id {
    octree::Id source_batch;
    uint32_t cluster_index;
};

struct ClusterBatch {
    Clustering clustering;
    std::vector<std::vector<Id>> child_map;

    static ClusterBatch make_leaves(Clustering clustering) {
        return {clustering, {}};
    }

    bool is_leaves() const {
        return this->child_map.empty();
    }
};

/*
std::vector<std::vector<uint32_t>> build_parent_map(const std::vector<std::vector<uint32_t>>& child_map) {
    std::vector<std::vector<uint32_t>> parent_map(child_map.size());
    for (const auto& [cluster_index, children] : enumerate(child_map)) {
        for (const uint32_t child_index : children) {
            auto &parents = parent_map[child_index];
            parents.push_back(cluster_index);
        }
    }
    return parent_map;
}*/

}

namespace dag {

template <typename Archive>
auto serialize(Archive &archive, const dag::Id &id) {
    return archive(id.source_batch, id.cluster_index);
}

template <typename Archive>
auto serialize(Archive &archive, dag::Id &id) {
    return archive(id.source_batch, id.cluster_index);
}

template <typename Archive>
auto serialize(Archive &archive, const dag::ClusterBatch &node) {
    return archive(node.clustering, node.child_map);
}

template <typename Archive>
auto serialize(Archive &archive, dag::ClusterBatch &node) {
    return archive(node.clustering, node.child_map);
}
} // namespace dag

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

}

namespace zpp::bits {

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
}

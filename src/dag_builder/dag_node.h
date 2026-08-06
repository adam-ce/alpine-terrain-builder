#pragma once

#include "cluster.h"
#include "dag_id.h"
#include "metadata.h"

namespace dag {

// A batch of dag nodes.
struct ClusterBatch {
    NodeMetadata metadata;
    Clustering clustering;
};

inline ClusterBatch make_leaf_batch(Clustering clustering) {
    NodeMetadata metadata = build_leaf_metadata(clustering);
    return {std::move(metadata), std::move(clustering)};
}

}

namespace dag {

template <typename Archive>
auto serialize(Archive &archive, const dag::ClusterBatch &node) {
    return archive(node.metadata, node.clustering);
}

template <typename Archive>
auto serialize(Archive &archive, dag::ClusterBatch &node) {
    return archive(node.metadata, node.clustering);
}

} // namespace dag

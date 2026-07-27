#pragma once

#include "cluster.h"
#include "meshopt.h"

inline void optimize_inplace(Cluster &cluster) {
    meshopt::optimize_meshlet(cluster.vertex_indices, cluster.local_triangles);
}

inline Cluster optimize(Cluster cluster) {
    optimize_inplace(cluster);
    return cluster;
}

inline void optimize_inplace(Clustering &clustering) {
    for (auto &cluster : clustering.clusters) {
        optimize_inplace(cluster);
    }
}

inline Clustering optimize(Clustering clustering) {
    optimize_inplace(clustering);
    return clustering;
}

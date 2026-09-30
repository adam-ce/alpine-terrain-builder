/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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

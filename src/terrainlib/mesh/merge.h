#pragma once

#include <unordered_map>
#include <span>
#include <optional>
#include <vector>

#include "mesh/merging/VertexDeduplicate.h"
#include "mesh/merging/VertexMapping.h"
#include "pch.h"

namespace mesh {

// SimpleMesh merge(const std::span<const std::reference_wrapper<const SimpleMesh>> meshes);
SimpleMesh merge(const std::span<const std::reference_wrapper<const SimpleMesh>> meshes, double distance_epsilon);
SimpleMesh merge(const std::span<const std::reference_wrapper<const SimpleMesh>> meshes, merging::VertexDeduplicate<3, double, merging::VertexId> &deduplicate);

template <typename... Args>
SimpleMesh merge(const SimpleMesh &mesh1, const SimpleMesh &mesh2, Args &&...args) {
    const std::array<std::reference_wrapper<const SimpleMesh>, 2> meshes = {mesh1, mesh2};
    return merge(std::span<const std::reference_wrapper<const SimpleMesh>>(meshes), std::forward<Args>(args)...);
}
template <typename... Args>
SimpleMesh merge(const SimpleMesh &mesh1, const SimpleMesh &mesh2, const SimpleMesh &mesh3, Args &&...args) {
    const std::array<std::reference_wrapper<const SimpleMesh>, 3> meshes = {mesh1, mesh2, mesh3};
    return merge(std::span<const std::reference_wrapper<const SimpleMesh>>(meshes), std::forward<Args>(args)...);
}
template <typename... Args>
SimpleMesh merge(const SimpleMesh &mesh1, const SimpleMesh &mesh2, const SimpleMesh &mesh3, const SimpleMesh& mesh4, Args &&...args) {
    const std::array<std::reference_wrapper<const SimpleMesh>, 4> meshes = {mesh1, mesh2, mesh3, mesh4};
    return merge(std::span<const std::reference_wrapper<const SimpleMesh>>(meshes), std::forward<Args>(args)...);
}

}

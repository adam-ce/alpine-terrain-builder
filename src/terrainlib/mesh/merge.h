/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Martin Braunsperger
 * Copyright (C) 2024 Adam Celarek-Litofcenko
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

#include <span>
#include <vector>
#include <array>
#include <functional>

#include "mesh/merging/VertexMapping.h"
#include "mesh/merging/mapping.h"

namespace mesh {

template <typename Mode = merging::EstimateEpsilon>
SimpleMesh merge(
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes,
    const merging::CreateOptions<Mode> create_options = merging::create_options(),
    const merging::ApplyOptions apply_options = merging::apply_options()) {
    switch (meshes.size()) {
    case 0:
        return {};
    case 1:
        return meshes[0];
    default:
        const merging::VertexMapping mapping = merging::create_mapping(meshes, create_options);
        return merging::apply_mapping(meshes, mapping, apply_options);
    }
}

template <std::size_t N, typename... Args>
SimpleMesh merge(const std::span<const SimpleMesh, N> meshes, Args &&...args) {
    std::array<std::reference_wrapper<const SimpleMesh>, N> refs;
    for (size_t i=0; i<meshes.size(); i++) {
        refs[i] = std::cref(meshes[i]);
    }
    return merge(std::span<const std::reference_wrapper<const SimpleMesh>, N>(refs), std::forward<Args>(args)...);
}
template <typename... Args>
SimpleMesh merge(const std::span<const SimpleMesh> meshes, Args &&...args) {
    std::vector<std::reference_wrapper<const SimpleMesh>> refs;
    refs.reserve(meshes.size());
    for (const auto &mesh : meshes) {
        refs.push_back(std::cref(mesh));
    }
    return merge(std::span<const std::reference_wrapper<const SimpleMesh>>(refs), std::forward<Args>(args)...);
}

template <typename T>
concept SimpleMeshRefConcept =
    std::is_same_v<T, SimpleMesh> ||
    std::is_same_v<T, const SimpleMesh &> ||
    std::is_same_v<T, SimpleMesh &> ||
    std::is_same_v<T, std::reference_wrapper<const SimpleMesh>> ||
    std::is_same_v<T, std::reference_wrapper<SimpleMesh>>;

template <SimpleMeshRefConcept Mesh, typename... Args>
SimpleMesh merge(Mesh&& mesh1, Args &&... /*args*/) {
    return mesh1;
}
template <SimpleMeshRefConcept Mesh1, SimpleMeshRefConcept Mesh2, typename... Args>
SimpleMesh merge(Mesh1&& mesh1, Mesh2&& mesh2, Args &&...args) {
    const std::array<std::reference_wrapper<const SimpleMesh>, 2> meshes = {mesh1, mesh2};
    return merge(std::span<const std::reference_wrapper<const SimpleMesh>>(meshes), std::forward<Args>(args)...);
}
template <SimpleMeshRefConcept Mesh1, SimpleMeshRefConcept Mesh2, SimpleMeshRefConcept Mesh3, typename... Args>
SimpleMesh merge(Mesh1&& mesh1, Mesh2&& mesh2, Mesh3&& mesh3, Args &&...args) {
    const std::array<std::reference_wrapper<const SimpleMesh>, 3> meshes = {mesh1, mesh2, mesh3};
    return merge(std::span<const std::reference_wrapper<const SimpleMesh>>(meshes), std::forward<Args>(args)...);
}
template <SimpleMeshRefConcept Mesh1, SimpleMeshRefConcept Mesh2, SimpleMeshRefConcept Mesh3, SimpleMeshRefConcept Mesh4, typename... Args>
SimpleMesh merge(Mesh1&& mesh1, Mesh2&& mesh2, Mesh3&& mesh3, Mesh4&& mesh4, Args &&...args) {
    const std::array<std::reference_wrapper<const SimpleMesh>, 4> meshes = {mesh1, mesh2, mesh3, mesh4};
    return merge(std::span<const std::reference_wrapper<const SimpleMesh>>(meshes), std::forward<Args>(args)...);
}
}

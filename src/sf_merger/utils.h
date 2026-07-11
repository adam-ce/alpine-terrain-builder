#pragma once

#include <glm/common.hpp>
#include <radix/geometry.h>

#include "Cow.h"
#include "mask.h"
#include "mesh/SimpleMesh.h"
#include "mesh/clip.h"
#include "mesh/texture_trim.h"

inline Cow<const SimpleMesh> clip_on_mask(const SimpleMesh &mesh, const MeshMask &mask, const bool keep_inside = true) {
    Cow<const SimpleMesh> clipped = mesh::clip_on_mesh(mesh, mask.mesh, keep_inside);
    if (clipped.is_ref()) {
        return clipped;
    }
    SimpleMesh result = clipped.get();
    trim_texture_inplace(result);
    return Cow<const SimpleMesh>::from_owned(std::move(result));
}

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> pad_bounds(const radix::geometry::Aabb<n_dims, T> &bounds, const T factor) {
    using Vec = glm::vec<n_dims, T>;
    const Vec center = bounds.centre();
    const Vec half_size = bounds.size() * (factor / T(2));
    const Vec new_min = center - half_size;
    const Vec new_max = center + half_size;
    return radix::geometry::Aabb<n_dims, T>(new_min, new_max);
}

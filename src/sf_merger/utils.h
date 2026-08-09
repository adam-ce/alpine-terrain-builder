#pragma once

#include <glm/common.hpp>
#include <radix/geometry.h>

#include "containers/Cow.h"
#include "geometry/geometry.h"
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

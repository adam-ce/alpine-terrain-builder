#pragma once

#include "Cow.h"
#include "mask.h"
#include "mesh/SimpleMesh.h"
#include "mesh/clip.h"

inline Cow<const SimpleMesh> clip_on_mask(const SimpleMesh &mesh, const MeshMask &mask) {
    return mesh::clip_on_mesh(mesh, mask.mesh);
}

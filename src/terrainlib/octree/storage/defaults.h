#pragma once

#include "mesh/SimpleMesh.h"
#include "codec/Codec.h"
#include "codec/MeshCodec.h"
#include "codec/DefaultCodec.h"

namespace octree {
    
using DefaultT = mesh::Simple;
template <typename T>
using DefaultCodecFor = std::conditional_t<
    std::is_same_v<T, mesh::Simple>,
    MeshCodec,
    DefaultCodec<T>>;

} // namespace octree
#pragma once

#include "mesh/SimpleMesh.h"
#include "codec/Codec.h"
#include "codec/MeshCodec.h"
#include "codec/ZppBitsCodec.h"

namespace octree {
    
using DefaultT = mesh::Simple;

template <typename T>
struct DefaultCodec {
    using type = ZppBitsCodec<T>;
};

template <>
struct DefaultCodec<mesh::Simple> {
    using type = MeshCodec;
};

template <typename T>
using DefaultCodecFor = typename DefaultCodec<std::remove_cvref_t<T>>::type;

} // namespace octree
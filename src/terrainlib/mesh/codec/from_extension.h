#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <expected>

#include "mesh/codec/Gltf.h"
#include "mesh/codec/Terrain.h"
#include "store/Codec.h"

namespace mesh::codec {

inline std::expected<std::unique_ptr<store::Codec<mesh::Simple>>, store::CodecError>
from_extension(const std::string_view extension) {
    if (extension == ".terrain") {
        return std::make_unique<Terrain>();
    }
    if (extension == ".glb") {
        return std::make_unique<Gltf>(GltfContainer::Binary);
    }
    if (extension == ".gltf") {
        return std::make_unique<Gltf>(GltfContainer::Json);
    }
    return std::unexpected(store::CodecError{
        store::CodecOperation::Resolve,
        store::CodecErrorCategory::UnsupportedCodec,
        "unsupported mesh codec selector: " + std::string(extension),
    });
}

} // namespace mesh::codec

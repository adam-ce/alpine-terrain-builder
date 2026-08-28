#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <expected>

#include "mesh/codec/Gltf.h"
#include "mesh/codec/SfMesh.h"
#include "store/Codec.h"

namespace mesh::codec {

inline Expected<std::unique_ptr<store::Codec<mesh::Simple>>> from_extension(const std::string_view extension)
{
    if (extension == ".sfmesh") {
        return std::make_unique<SfMesh>();
    }
    if (extension == ".glb") {
        return std::make_unique<Gltf>(GltfContainer::Binary);
    }
    if (extension == ".gltf") {
        return std::make_unique<Gltf>(GltfContainer::Json);
    }
    return Error::fail(Error::Code::Unsupported, "unsupported mesh codec selector: " + std::string(extension));
}

} // namespace mesh::codec

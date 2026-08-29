#include "mesh/io.h"
#include "log.h"
#include "mesh/io/gltf.h"
#include "mesh/codec/SfMesh.h"
#include "mesh/validate.h"

namespace mesh::io {

Expected<SimpleMesh> load_from_path(
    const std::filesystem::path &path,
    const LoadOptions& options) {
    const std::filesystem::path extension = path.extension();
    if (extension == ".glb" || extension == ".gltf") {
        return gltf::load_from_path(path, options);
    } else if (extension == ".sfmesh") {
        std::filesystem::path node_path = path;
        node_path.replace_extension();
        const mesh::codec::SfMesh codec;
        return codec.read(node_path);
    } else {
        return Error::fail(Error::Code::Unsupported, "unsupported mesh input format: " + extension.string());
    }
}

Expected<void> save_to_path(
    const SimpleMesh &mesh,
    const std::filesystem::path &path,
    const SaveOptions &options) {
    LOG_TRACE("Saving mesh to path {}", path);

    mesh::validate_basic(mesh);

    const std::filesystem::path extension = path.extension();
    if (extension == ".glb" || extension == ".gltf") {
        return gltf::save_to_path(mesh, path, options);
    } else if (extension == ".sfmesh") {
        std::filesystem::path node_path = path;
        node_path.replace_extension();
        const mesh::codec::SfMesh codec;
        return codec.write(node_path, mesh, mesh::EncodeOptions{.texture_format = options.texture_format});
    } else {
        return Error::fail(Error::Code::Unsupported, "unsupported mesh output format: " + extension.string());
    }
}

}

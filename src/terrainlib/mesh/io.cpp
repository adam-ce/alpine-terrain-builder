#include "mesh/io.h"
#include "log.h"
#include "mesh/io/gltf.h"
#include "mesh/io/terrain.h"

namespace mesh {
namespace io {

tl::expected<SimpleMesh, LoadMeshError> load_from_path(
    const std::filesystem::path &path,
    const LoadOptions& options) {
    const std::filesystem::path extension = path.extension();
    if (extension == ".glb" || extension == ".gltf") {
        return gltf::load_from_path(path, options);
    } else if (extension == ".terrain") {
        return terrain::load_from_path(path, options);
    } else {
        return tl::unexpected(LoadMeshErrorKind::UnsupportedFormat);
    }
}

tl::expected<void, SaveMeshError> save_to_path(
    const SimpleMesh &mesh,
    const std::filesystem::path &path,
    const SaveOptions &options) {
    LOG_TRACE("Saving mesh to path {}", path.string());

    const std::filesystem::path extension = path.extension();
    if (extension == ".glb" || extension == ".gltf") {
        return gltf::save_to_path(mesh, path, options);
    } else if (extension == ".terrain") {
        return terrain::save_to_path(mesh, path, options);
    } else {
        return tl::unexpected(SaveMeshErrorKind::UnsupportedFormat);
    }
}

}
}

#include "mesh/io.h"
#include "log.h"
#include "mesh/io/gltf.h"
#include "mesh/codec/SfMesh.h"
#include "mesh/validate.h"

namespace mesh::io {

namespace {

Error load_error(const LoadMeshError error, const std::filesystem::path& path)
{
    switch (static_cast<LoadMeshErrorKind>(error)) {
    case LoadMeshErrorKind::UnsupportedFormat:
        return Error::make(Error::Code::Unsupported, "load unsupported glTF format from", path);
    case LoadMeshErrorKind::FileNotFound:
        return Error::make(Error::Code::NotFound, "open glTF file", path);
    case LoadMeshErrorKind::InvalidFormat:
        return Error::make(Error::Code::CorruptData, "decode invalid glTF file", path);
    case LoadMeshErrorKind::OutOfMemory:
        return Error::make(Error::Code::ResourceExhausted, "load glTF file: out of memory", path);
    }
    return Error::make(Error::Code::Internal, "unknown glTF load failure for \"" + path.string() + "\"");
}

Error save_error(const SaveMeshError error, const std::filesystem::path& path)
{
    switch (static_cast<SaveMeshErrorKind>(error)) {
    case SaveMeshErrorKind::UnsupportedFormat:
        return Error::make(Error::Code::Unsupported, "write unsupported glTF format to", path);
    case SaveMeshErrorKind::OpenFile:
        return Error::make(Error::Code::Io, "open glTF file for writing", path);
    case SaveMeshErrorKind::WriteFile:
        return Error::make(Error::Code::Io, "write glTF file", path);
    case SaveMeshErrorKind::OutOfMemory:
        return Error::make(Error::Code::ResourceExhausted, "write glTF file: out of memory", path);
    }
    return Error::make(Error::Code::Internal, "unknown glTF write failure for \"" + path.string() + "\"");
}

} // namespace

Expected<SimpleMesh> load_from_path(
    const std::filesystem::path &path,
    const LoadOptions& options) {
    const std::filesystem::path extension = path.extension();
    if (extension == ".glb" || extension == ".gltf") {
        auto result = gltf::load_from_path(path, options);
        if (!result) {
            return Error::propagate(load_error(result.error(), path), "load glTF mesh");
        }
        return std::move(*result);
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
        auto result = gltf::save_to_path(mesh, path, options);
        if (!result) {
            return Error::propagate(save_error(result.error(), path), "write glTF mesh");
        }
        return {};
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

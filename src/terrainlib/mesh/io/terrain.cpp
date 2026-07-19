#include <fstream>

#include <libassert/assert.hpp>

#include "io/bytes.h"
#include "log.h"
#include "mesh/io/terrain.h"
#include "mesh/EncodedMesh.h"
#include "mesh/encode.h"

namespace mesh::io::terrain {

namespace {
LoadMeshError load_error_from_io_error(::io::Error error) {
    switch (error) {
    case ::io::Error::Value::OpenFile:
        return LoadMeshErrorKind::FileNotFound;
    case ::io::Error::Value::DetermineSize:
    case ::io::Error::Value::ReadBytes:
        return LoadMeshErrorKind::InvalidFormat;
    default:
        return LoadMeshErrorKind::InvalidFormat;
    }
}

SaveMeshError save_error_from_io_error(::io::Error error) {
    switch (error) {
    case ::io::Error::Value::OpenFile:
        return SaveMeshErrorKind::OpenFile;
    case ::io::Error::Value::WriteBytes:
        return SaveMeshErrorKind::WriteFile;
    default:
        return SaveMeshErrorKind::WriteFile;
    }
}

std::expected<void, SaveMeshError> write_bytes_to_path(
    const std::span<const uint8_t> bytes, const std::filesystem::path &path) {
    const auto result = ::io::write_bytes_to_path(bytes, path);
    if (!result.has_value()) {
        return std::unexpected(save_error_from_io_error(result.error()));
    }
    return {};
}

std::expected<std::vector<uint8_t>, LoadMeshError> read_bytes_from_path(const std::filesystem::path &path) {
    const auto result = ::io::read_bytes_from_path(path);
    if (!result.has_value()) {
        return std::unexpected(load_error_from_io_error(result.error()));
    }
    return result.value();
}

std::expected<std::vector<uint8_t>, SaveMeshError> save_encoded_to_buffer(const mesh::Encoded &mesh) {
    LOG_TRACE("Serializing mesh to buffer");

    // TODO: this ignores the texture format in SaveOptions
    std::vector<uint8_t> data;
    zpp::bits::out out(data);
    auto result = out(mesh);
    if (zpp::bits::failure(result)) {
        std::error_code error_code = std::make_error_code(result);
        LOG_ERROR("Error while writing mesh: {}", error_code.message());

        switch (result) {
        case std::errc::no_buffer_space:
        case std::errc::message_size:
        case std::errc::result_out_of_range:
            return std::unexpected(SaveMeshErrorKind::OutOfMemory);
            break;
        default:
            UNREACHABLE();
            break;
        }
    }

    return data;
}

std::expected<mesh::Encoded, LoadMeshError> load_encoded_from_buffer(const std::span<const uint8_t> bytes) {
    LOG_TRACE("Deserializing mesh from buffer");

    zpp::bits::in in(bytes);
    mesh::Encoded mesh;
    auto result = in(mesh);
    if (zpp::bits::failure(result)) {
        std::error_code error_code = std::make_error_code(result);
        LOG_ERROR("error while reading mesh: {}", error_code.message());

        switch (result) {
        case std::errc::no_buffer_space:
        case std::errc::message_size:
            return std::unexpected(LoadMeshErrorKind::OutOfMemory);
        case std::errc::value_too_large:
        case std::errc::bad_message:
        case std::errc::protocol_error:
        case std::errc::result_out_of_range:
            return std::unexpected(LoadMeshErrorKind::InvalidFormat);
        case std::errc::not_supported:
        case std::errc::invalid_argument:
            UNREACHABLE();
            break;
        default:
            throw std::runtime_error("unexpected error");
        }
    }

    return mesh;
}
}

std::expected<std::vector<uint8_t>, SaveMeshError> save_to_buffer(const SimpleMesh &mesh, const SaveOptions& options) {
    const auto encode_result = mesh::encode(mesh, mesh::EncodeOptions{
                                                      .texture_format = options.texture_format});
    if (!encode_result.has_value()) {
        return std::unexpected(SaveMeshErrorKind::UnsupportedFormat);
    }
    const mesh::Encoded encoded = encode_result.value();

    const auto deser_result = save_encoded_to_buffer(encoded);
    if (!deser_result.has_value()) {
        return std::unexpected(deser_result.error());
    }
    const std::vector<uint8_t> buffer = deser_result.value();

    return buffer;
}

std::expected<SimpleMesh, LoadMeshError> load_from_buffer(const std::span<const uint8_t> bytes, const LoadOptions & /* options */) {
    const auto deser_result = load_encoded_from_buffer(bytes);
    if (!deser_result.has_value()) {
        return std::unexpected(deser_result.error());
    }
    const mesh::Encoded encoded = deser_result.value();

    const auto decode_result = mesh::decode(encoded);
    if (!decode_result.has_value()) {
        return std::unexpected(LoadMeshErrorKind::InvalidFormat);
    }
    const mesh::Simple mesh = decode_result.value();

    return mesh;
}

std::expected<SimpleMesh, LoadMeshError> load_from_path(const std::filesystem::path &path, const LoadOptions & /* options */) {
    const auto bytes_result = read_bytes_from_path(path);
    if (!bytes_result.has_value()) {
        return std::unexpected(bytes_result.error());
    }
    const std::vector<uint8_t> bytes = bytes_result.value();

    return load_from_buffer(bytes);
}

std::expected<void, SaveMeshError> save_to_path(const SimpleMesh &mesh, const std::filesystem::path &path, const SaveOptions& options) {
    LOG_TRACE("Saving mesh as high precision terrain");

    const auto result = save_to_buffer(mesh, options);
    if (!result.has_value()) {
        return std::unexpected(result.error());
    }
    const std::vector<uint8_t> bytes = result.value();

    const auto write_result = write_bytes_to_path(bytes, path);
    if (!write_result.has_value()) {
        return std::unexpected(write_result.error());
    }

    return {};
}

} // namespace mesh::io::terrain

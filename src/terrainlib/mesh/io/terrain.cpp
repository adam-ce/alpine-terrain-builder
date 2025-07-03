#include <fstream>

#include <libassert/assert.hpp>

#include "log.h"
#include "mesh/io/terrain.h"
#include "mesh/io/utils.h"

using namespace mesh::io::utils;

namespace mesh::io::terrain {

namespace {
tl::expected<void, SaveMeshError> write_bytes_to_path(
    const std::span<const uint8_t> bytes, const std::filesystem::path &path) {
    LOG_TRACE("Writing bytes to path {}", path);

    std::ofstream ofs(path, std::ios::out | std::ios::binary);
    if (!ofs.is_open()) {
        LOG_ERROR("Failed to open file {}", path);
        return tl::unexpected(SaveMeshErrorKind::OpenFile);
    }

    const unsigned long data_size = bytes.size();
    // ofs.write(reinterpret_cast<const char *>(&data_size), sizeof(unsigned long));
    ofs.write(reinterpret_cast<const char *>(bytes.data()), data_size);

    if (!ofs.good()) {
        LOG_ERROR("Failed to write to file {}", path);
        ofs.close();
        return tl::unexpected(SaveMeshErrorKind::WriteFile);
    }

    ofs.close();

    return {};
}

tl::expected<std::vector<uint8_t>, LoadMeshError> read_bytes_from_path(const std::filesystem::path &path) {
    LOG_TRACE("Reading bytes from path {}", path);

    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.is_open()) {
        LOG_ERROR("Failed to open file {}", path);
        return tl::unexpected(LoadMeshErrorKind::FileNotFound);
    }

    std::vector<uint8_t> data;

    // get length of file
    ifs.seekg(0, ifs.end);
    const size_t length = ifs.tellg();
    ifs.seekg(0, ifs.beg);

    // read file
    if (length > 0) {
        data.resize(length);
        ifs.read(reinterpret_cast<char *>(data.data()), length);
    }
    ifs.close();

    return data;
}
}

tl::expected<SimpleMesh, LoadMeshError> load_from_path(const std::filesystem::path &path, const LoadOptions& options) {
    const auto result = read_bytes_from_path(path);
    if (!result.has_value()) {
        return tl::unexpected(result.error());
    }
    const std::vector<uint8_t> bytes = result.value();
    return load_from_buffer(bytes, options);
}

tl::expected<std::vector<uint8_t>, SaveMeshError> save_to_buffer(const SimpleMesh &mesh, const SaveOptions& /* options */) {
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
            return tl::unexpected(SaveMeshErrorKind::OutOfMemory);
            break;
        default:
            UNREACHABLE();
            break;
        }
    }

    return data;
}

tl::expected<SimpleMesh, LoadMeshError> load_from_buffer(const std::span<const uint8_t> bytes, const LoadOptions & /* options */) {
    LOG_TRACE("Deserializing mesh from buffer");

    zpp::bits::in in(bytes);
    SimpleMesh mesh;
    auto result = in(mesh);
    if (zpp::bits::failure(result)) {
        std::error_code error_code = std::make_error_code(result);
        LOG_ERROR("error while reading mesh: {}", error_code.message());

        switch (result) {
        case std::errc::no_buffer_space:
        case std::errc::message_size:
            return tl::unexpected(LoadMeshErrorKind::OutOfMemory);
        case std::errc::value_too_large:
        case std::errc::bad_message:
        case std::errc::protocol_error:
        case std::errc::result_out_of_range:
            return tl::unexpected(LoadMeshErrorKind::InvalidFormat);
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

tl::expected<void, SaveMeshError> save_to_path(const SimpleMesh &mesh, const std::filesystem::path &path, const SaveOptions& options) {
    LOG_TRACE("Saving mesh as high precision mesh");

    const auto result = save_to_buffer(mesh, options);
    if (!result.has_value()) {
        return tl::unexpected(result.error());
    }
    const std::vector<uint8_t> bytes = result.value();

    create_parent_directories(path);

    return write_bytes_to_path(bytes, path);
}

} // namespace mesh::io::terrain

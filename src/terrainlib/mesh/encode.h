#pragma once

#include <ostream>
#include <vector>

#include <meshoptimizer.h>
#include <expected>

#include "log.h"
#include "mesh/EncodedMesh.h"
#include "mesh/SimpleMesh.h"
#include "mesh/io/texture.h"

namespace mesh {

namespace {
constexpr uint32_t GL_BYTE = 0x1400;
constexpr uint32_t GL_UNSIGNED_BYTE = 0x1401;
constexpr uint32_t GL_SHORT = 0x1402;
constexpr uint32_t GL_UNSIGNED_SHORT = 0x1403;
constexpr uint32_t GL_INT = 0x1404;
constexpr uint32_t GL_UNSIGNED_INT = 0x1405;
constexpr uint32_t GL_FLOAT = 0x1406;
constexpr uint32_t GL_DOUBLE = 0x140A;
}

template <typename T>
constexpr uint32_t component_type_id() {
    if constexpr (std::is_same_v<T, float>) {
        return GL_FLOAT;
    } else if constexpr (std::is_same_v<T, double>) {
        return GL_DOUBLE;
    } else if constexpr (std::is_same_v<T, int>) {
        return GL_INT;
    } else if constexpr (std::is_same_v<T, unsigned int>) {
        return GL_UNSIGNED_INT;
    } else if constexpr (std::is_same_v<T, short>) {
        return GL_SHORT;
    } else if constexpr (std::is_same_v<T, unsigned short>) {
        return GL_UNSIGNED_SHORT;
    } else if constexpr (std::is_same_v<T, char>) {
        return GL_BYTE;
    } else if constexpr (std::is_same_v<T, unsigned char>) {
        return GL_UNSIGNED_BYTE;
    } else {
        return -1;
    }
}

struct EncodeOptions {
    // Texture format
    std::string texture_format = ".jpeg";
};

enum class EncodeError {
    PositionEncode,
    UvEncode,
    TriangleEncode,
    TextureEncode
};

inline std::ostream& operator<<(std::ostream& os, const EncodeError& err) {
    switch (err) {
    case EncodeError::PositionEncode:
        os << "Position encoding failed";
        break;
    case EncodeError::UvEncode:
        os << "UV encoding failed";
        break;
    case EncodeError::TriangleEncode:
        os << "Triangle encoding failed";
        break;
    case EncodeError::TextureEncode:
        os << "Texture encoding failed";
        break;
    default:
        os << "Unknown EncodeError";
        break;
    }
    return os;
}

template <glm::length_t n_dims, typename T>
std::expected<Encoded, EncodeError> encode(const Simple_<n_dims, T>& mesh, const EncodeOptions options = {}) {
    using Mesh = Simple_<n_dims, T>;

    const size_t vertex_count = mesh.vertex_count();
    const size_t position_size = sizeof(typename Mesh::Position);
    const size_t uv_size = sizeof(typename Mesh::Uv);
    const size_t face_count = mesh.face_count();
    const size_t index_count = face_count * 3;

    // Encode positions
    std::vector<uint8_t> position_buf;
    if (vertex_count > 0) {
        position_buf.resize(meshopt_encodeVertexBufferBound(vertex_count, position_size));
        const size_t pos_written = meshopt_encodeVertexBuffer(position_buf.data(), position_buf.size(), mesh.positions.data(), vertex_count, position_size);
        if (pos_written == 0) {
            return std::unexpected(EncodeError::PositionEncode);
        }
        position_buf.resize(pos_written);
    }

    // Encode uvs
    std::vector<uint8_t> uv_buf;
    if (mesh.has_uvs()) {
        uv_buf.resize(meshopt_encodeVertexBufferBound(vertex_count, uv_size));
        const size_t uv_written = meshopt_encodeVertexBuffer(uv_buf.data(), uv_buf.size(), mesh.uvs.data(), vertex_count, uv_size);
        if (uv_written == 0) {
            return std::unexpected(EncodeError::UvEncode);
        }
        uv_buf.resize(uv_written);
    }

    // Encode triangles
    std::vector<uint8_t> index_buf;
    if (face_count > 0) {
        index_buf.resize(meshopt_encodeIndexBufferBound(index_count, vertex_count));
        const size_t index_written = meshopt_encodeIndexBuffer(
            index_buf.data(), index_buf.size(),
            reinterpret_cast<const unsigned int *>(mesh.triangles.data()),
            index_count);
        if (index_written == 0) {
            return std::unexpected(EncodeError::TriangleEncode);
        }
        index_buf.resize(index_written);
    }

    // Encode texture
    std::vector<uint8_t> texture_buf;
    if (mesh.has_texture()) {
        try {
            texture_buf = mesh::io::write_texture_to_encoded_buffer(mesh.texture.value(), options.texture_format);
        } catch (const cv::Exception &e) {
            LOG_ERROR("Failed while encoding texture {}", e.what());
            return std::unexpected(EncodeError::TextureEncode);
        }
    }

    const Encoded::Header header{
        .version = 1,
        .n_dims = n_dims,
        .component_type = component_type_id<T>(),
        .vertex_count = static_cast<uint32_t>(vertex_count),
        .face_count = static_cast<uint32_t>(mesh.face_count())};

    return Encoded{
        .header = header,
        .triangles = index_buf,
        .positions = position_buf,
        .uvs = uv_buf,
        .texture = texture_buf};
}

struct DecodeOptions {
};

enum class DecodeError {
    IncompatibleData,
    PositionDecode,
    UvDecode,
    TriangleDecode,
    TextureDecode
};

inline std::ostream &operator<<(std::ostream &os, const DecodeError &err) {
    switch (err) {
    case DecodeError::IncompatibleData:
        os << "Incompatible data";
        break;
    case DecodeError::PositionDecode:
        os << "Position decoding failed";
        break;
    case DecodeError::UvDecode:
        os << "UV decoding failed";
        break;
    case DecodeError::TriangleDecode:
        os << "Triangle decoding failed";
        break;
    case DecodeError::TextureDecode:
        os << "Texture decoding failed";
        break;
    default:
        os << "Unknown DecodeError";
        break;
    }
    return os;
}


template <glm::length_t n_dims = 3, typename T = double>
std::expected<Simple_<n_dims, T>, DecodeError> decode(const Encoded &encoded, const DecodeOptions = {}) {
    const uint32_t expected_component_type = component_type_id<T>();
    const Encoded::Header& header = encoded.header;
    if (header.version != 1 ||
        header.n_dims != n_dims ||
        header.component_type != expected_component_type) {
        return std::unexpected(DecodeError::IncompatibleData);
    }

    using Mesh = Simple_<n_dims, T>;
    Mesh mesh;
    int result;

    // Decode positions
    const size_t position_size = sizeof(typename Mesh::Position);
    const size_t vertex_count = header.vertex_count;
    mesh.positions.resize(vertex_count);
    result = meshopt_decodeVertexBuffer(mesh.positions.data(), vertex_count, position_size, encoded.positions.data(), encoded.positions.size());
    if (result != 0) {
        return std::unexpected(DecodeError::PositionDecode);
    }

    // Decode uvs
    if (!encoded.uvs.empty()) {
        const size_t uv_size = sizeof(typename Mesh::Uv);
        mesh.uvs.resize(vertex_count);
        result = meshopt_decodeVertexBuffer(mesh.uvs.data(), vertex_count, uv_size, encoded.uvs.data(), encoded.uvs.size());
        if (result != 0) {
            return std::unexpected(DecodeError::UvDecode);
        }
    }

    // Decode triangles
    const size_t face_count = header.face_count;
    const size_t index_count = face_count * 3;
    const size_t index_size = sizeof(typename Mesh::Triangle::value_type);
    mesh.triangles.resize(face_count);
    result = meshopt_decodeIndexBuffer(mesh.triangles.data(), index_count, index_size, encoded.triangles.data(), encoded.triangles.size());
    if (result != 0) {
        return std::unexpected(DecodeError::TriangleDecode);
    }

    // Decode texture
    if (!encoded.texture.empty()) {
        try {
            mesh.texture = mesh::io::read_texture_from_encoded_bytes(encoded.texture);
        } catch (const cv::Exception &e) {
            LOG_ERROR("Failed while decoding texture {}", e.what());
            return std::unexpected(DecodeError::TextureDecode);
        }
    }

    return mesh;
}
}

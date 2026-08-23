#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <expected>

#include "io/envelope.h"
#include "mesh/EncodedMesh.h"
#include "mesh/SimpleMesh.h"
#include "mesh/encode.h"

namespace mesh::sf {

namespace v1 {

    struct Payload {
        std::uint32_t vertex_count;
        std::uint32_t face_count;
        std::vector<std::uint8_t> triangles;
        std::vector<std::uint8_t> positions;
        std::vector<std::uint8_t> uvs;
        std::vector<std::uint8_t> texture;
    };

} // namespace v1

using Schema = ::io::envelope::PayloadSchema<"mesh.SfMesh", ::io::envelope::Version<1, v1::Payload>>;
using Payload = Schema::latest_type;

inline std::expected<void, std::string> validate(const Payload& payload)
{
    if (payload.vertex_count > ::io::envelope::default_max_decompressed_size / sizeof(mesh::Simple::Position)) {
        return std::unexpected("SF mesh vertex count exceeds the allocation limit");
    }
    if (payload.face_count > ::io::envelope::default_max_decompressed_size / sizeof(mesh::Simple::Triangle)) {
        return std::unexpected("SF mesh face count exceeds the allocation limit");
    }
    if (payload.vertex_count == 0 && (!payload.positions.empty() || !payload.uvs.empty())) {
        return std::unexpected("empty mesh contains vertex data");
    }
    if (payload.vertex_count != 0 && payload.positions.empty()) {
        return std::unexpected("non-empty mesh contains no position data");
    }
    if (payload.face_count == 0 && !payload.triangles.empty()) {
        return std::unexpected("empty mesh contains triangle data");
    }
    if (payload.face_count != 0 && payload.triangles.empty()) {
        return std::unexpected("non-empty mesh contains no triangle data");
    }
    return {};
}

inline std::expected<Payload, mesh::EncodeError> encode_payload(const mesh::Simple& mesh, const mesh::EncodeOptions options = {})
{
    auto encoded = mesh::encode(mesh, options);
    if (!encoded) {
        return std::unexpected(encoded.error());
    }
    return Payload {
        .vertex_count = encoded->header.vertex_count,
        .face_count = encoded->header.face_count,
        .triangles = std::move(encoded->triangles),
        .positions = std::move(encoded->positions),
        .uvs = std::move(encoded->uvs),
        .texture = std::move(encoded->texture),
    };
}

inline std::expected<mesh::Simple, mesh::DecodeError> decode_payload(Payload payload)
{
    const mesh::Encoded encoded{
        .header = {
            .version = 1,
            .n_dims = mesh::Simple::n_dims,
            .component_type = mesh::component_type_id<mesh::Simple::Component>(),
            .vertex_count = payload.vertex_count,
            .face_count = payload.face_count,
        },
        .triangles = std::move(payload.triangles),
        .positions = std::move(payload.positions),
        .uvs = std::move(payload.uvs),
        .texture = std::move(payload.texture),
    };
    return mesh::decode(encoded);
}

} // namespace mesh::sf

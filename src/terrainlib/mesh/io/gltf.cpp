#include <array>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <limits>
#include <new>
#include <stdexcept>

#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
// #define CGLTF_VALIDATE_ENABLE_ASSERTS
#include <cgltf_write.h>
#undef CGLTF_IMPLEMENTATION
#undef CGLTF_WRITE_IMPLEMENTATION

#include "io/utils.h"
#include "log.h"
#include "mesh/io/gltf.h"
#include "mesh/io/texture.h"

namespace mesh::io::gltf {

namespace {
cgltf_attribute *find_attribute_with_type(cgltf_attribute *attributes, size_t attribute_count, cgltf_attribute_type type) {
    for (unsigned int i = 0; i < attribute_count; i++) {
        cgltf_attribute *attribute = &attributes[i];
        if (attribute->type == type) {
            return attribute;
        }
    }

    return nullptr;
}

glm::mat4 get_node_transform_local(const cgltf_node &node) {
    if (node.has_matrix) {
        return glm::make_mat4(node.matrix);
    }

    glm::mat4 transform(1);

    if (node.has_translation) {
        const glm::vec3 translation(node.translation[0], node.translation[1], node.translation[2]);
        transform = glm::translate(transform, translation);
    }

    if (node.has_rotation) {
        glm::quat rotation(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
        transform = transform * glm::toMat4(rotation);
    }

    if (node.has_scale) {
        glm::vec3 scaling(node.scale[0], node.scale[1], node.scale[2]);
        transform = glm::scale(transform, scaling);
    }

    return transform;
}

glm::dmat4 get_node_transform_world(const cgltf_node &node) {
    glm::dmat4 transform = get_node_transform_local(node);

    const cgltf_node *parent = node.parent;
    while (parent != nullptr) {
        const glm::dmat4 parent_transform = get_node_transform_local(*parent);
        transform = parent_transform * transform;
        parent = parent->parent;
    }

    return transform;
}

const cgltf_node *find_mesh_node_under_node(const cgltf_node &node, const cgltf_mesh &target_mesh) {
    if (node.mesh != nullptr) {
        if (std::addressof(target_mesh) == node.mesh) {
            return &node;
        }
    }

    for (cgltf_size child_index = 0; child_index < node.children_count; child_index++) {
        const cgltf_node &child = *node.children[child_index];
        const cgltf_node *child_result = find_mesh_node_under_node(child, target_mesh);
        if (child_result != nullptr) {
            return child_result;
        }
    }

    return nullptr;
}
const cgltf_node *find_mesh_node_in_scene(const cgltf_scene &scene, const cgltf_mesh &mesh) {
    for (cgltf_size i = 0; i < scene.nodes_count; i++) {
        const cgltf_node &node = *scene.nodes[i];
        const cgltf_node *mesh_node = find_mesh_node_under_node(node, mesh);
        if (mesh_node != nullptr) {
            return mesh_node;
        }
    }

    return nullptr;
}
const cgltf_node *find_mesh_node(const cgltf_data &data, const cgltf_mesh &mesh) {
    if (data.scenes_count == 0) {
        LOG_WARN("file contains no scenes");
        return nullptr;
    }

    for (cgltf_size i = 0; i < data.scenes_count; i++) {
        const cgltf_scene &scene = data.scenes[i];
        const cgltf_node *mesh_node = find_mesh_node_in_scene(scene, mesh);
        if (mesh_node != nullptr) {
            return mesh_node;
        }
    }

    return nullptr;
}

glm::dmat4 get_mesh_transform(const cgltf_data &data, const cgltf_mesh &mesh) {
    const cgltf_node *mesh_node = find_mesh_node(data, mesh);
    if (mesh_node == nullptr) {
        return glm::dmat4(1);
    }
    return get_node_transform_world(*mesh_node);
}

/// Encodes the data at the given pointer into base64.
// from https://github.com/syoyo/tinygltf/blob/5e8a7fd602af22aa9619ccd3baeaeeaff0ecb6f3/tiny_gltf.h#L2297
std::string base64_encode(unsigned char const *bytes_to_encode,
                                 unsigned int in_len) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    const char *base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] =
                ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] =
                ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] =
            ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] =
            ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    return ret;
}

/// Encodes the given data as a base64 data uri.
std::string data_uri_encode(unsigned char const *bytes_to_encode, unsigned int in_len) {
    return "data:application/octet-stream;base64," + base64_encode(bytes_to_encode, in_len);
}

Expected<std::optional<cv::Mat>> load_texture_from_material(const cgltf_material &material) {
    if (!material.has_pbr_metallic_roughness || material.pbr_metallic_roughness.base_color_texture.texture == nullptr) {
        LOG_WARN("mesh material has no texture");
        return std::nullopt;
    }

    cgltf_texture &albedo_texture = *material.pbr_metallic_roughness.base_color_texture.texture;
    cgltf_image &albedo_image = *albedo_texture.image;

    if (albedo_image.buffer_view == nullptr) {
        LOG_WARN("mesh material texture image is not embedded in a buffer view");
        return std::nullopt;
    }

    const std::span<const uint8_t> raw_texture{cgltf_buffer_view_data(albedo_image.buffer_view), albedo_image.buffer_view->size};
    try {
        cv::Mat texture = mesh::io::read_texture_from_encoded_bytes(raw_texture);
        if (texture.empty()) {
            return Error::fail(Error::Code::CorruptData, "could not decode embedded glTF texture");
        }
        return std::optional<cv::Mat> { std::move(texture) };
    } catch (const cv::Exception& error) {
        if (error.code == cv::Error::StsNoMem) {
            return Error::fail(Error::Code::ResourceExhausted, "decode embedded glTF texture: out of memory");
        }
        return Error::fail(Error::Code::CorruptData, "could not decode embedded glTF texture: " + error.msg);
    }
}

Error load_cgltf_error(const cgltf_result result, const std::filesystem::path& path) {
    switch (result) {
    case cgltf_result::cgltf_result_data_too_short:
    case cgltf_result::cgltf_result_unknown_format:
    case cgltf_result::cgltf_result_invalid_json:
    case cgltf_result::cgltf_result_invalid_gltf:
        return Error::make(Error::Code::CorruptData, "decode invalid glTF file", path);
    case cgltf_result::cgltf_result_legacy_gltf:
        return Error::make(Error::Code::Unsupported, "decode legacy glTF file", path);
    case cgltf_result::cgltf_result_file_not_found:
        return Error::make(Error::Code::NotFound, "open glTF file", path);
    case cgltf_result::cgltf_result_io_error:
        return Error::make(Error::Code::Io, "read glTF file", path);
    case cgltf_result::cgltf_result_out_of_memory:
        return Error::make(Error::Code::ResourceExhausted, "load glTF file: out of memory", path);
    case cgltf_result::cgltf_result_invalid_options:
    case cgltf_result::cgltf_result_success:
    case cgltf_result::cgltf_result_max_enum:
        return Error::make(Error::Code::Internal, "unexpected glTF loader result for \"" + path.string() + "\"");
    }
    return Error::make(Error::Code::Internal, "unknown glTF loader result for \"" + path.string() + "\"");
}

Expected<std::vector<std::uint8_t>> serialize_json(const cgltf_options& options, const cgltf_data& data)
try {
    const cgltf_size expected_size = cgltf_write(&options, nullptr, 0, &data);
    if (expected_size == 0) {
        return Error::fail(Error::Code::Internal, "glTF serialization produced no output");
    }

    std::vector<std::uint8_t> json;
    if (expected_size > json.max_size()) {
        return Error::fail(Error::Code::ResourceExhausted, "serialized glTF exceeds the supported size");
    }
    json.resize(expected_size);
    const cgltf_size actual_size = cgltf_write(&options, reinterpret_cast<char*>(json.data()), json.size(), &data);
    if (actual_size != expected_size) {
        return Error::fail(Error::Code::Internal, "glTF serialization size changed between passes");
    }
    json.resize(actual_size - 1);
    return json;
} catch (const std::bad_alloc&) {
    return Error::fail(Error::Code::ResourceExhausted, "allocate glTF serialization output");
} catch (const std::length_error&) {
    return Error::fail(Error::Code::ResourceExhausted, "serialized glTF exceeds the supported size");
}

bool write_all(std::ofstream& file, std::span<const std::uint8_t> bytes)
{
    constexpr std::size_t max_write_size = std::size_t { 1 } << 30;
    while (!bytes.empty()) {
        const std::size_t write_size = (std::min)(bytes.size(), max_write_size);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(write_size));
        if (!file.good()) {
            return false;
        }
        bytes = bytes.subspan(write_size);
    }
    return true;
}

Expected<void> write_serialized(
    const std::filesystem::path& path,
    const std::span<const std::uint8_t> json,
    const cgltf_data& data,
    const bool binary_output)
try {
    constexpr std::uint32_t glb_magic = 0x46546C67;
    constexpr std::uint32_t glb_version = 2;
    constexpr std::uint32_t json_chunk_magic = 0x4E4F534A;
    constexpr std::uint32_t binary_chunk_magic = 0x004E4942;
    constexpr std::size_t header_size = 12;
    constexpr std::size_t chunk_header_size = 8;
    constexpr std::size_t glb_size_limit = (std::numeric_limits<std::uint32_t>::max)();

    std::size_t json_padding = 0;
    std::size_t binary_size = 0;
    std::size_t binary_padding = 0;
    std::size_t padded_json_size = 0;
    std::size_t padded_binary_size = 0;
    std::size_t total_size = json.size();
    if (binary_output) {
        json_padding = (4 - json.size() % 4) % 4;
        binary_size = data.bin == nullptr ? 0 : data.bin_size;
        binary_padding = (4 - binary_size % 4) % 4;
        if (json.size() > glb_size_limit - 3 || binary_size > glb_size_limit - 3) {
            return Error::fail(Error::Code::ResourceExhausted, "serialized glTF exceeds the GLB size limit");
        }
        padded_json_size = json.size() + json_padding;
        padded_binary_size = binary_size + binary_padding;
        if (padded_json_size > glb_size_limit - header_size - chunk_header_size) {
            return Error::fail(Error::Code::ResourceExhausted, "serialized glTF exceeds the GLB size limit");
        }
        total_size = header_size + chunk_header_size + padded_json_size;
        if (binary_size != 0) {
            if (padded_binary_size > glb_size_limit - chunk_header_size
                || chunk_header_size + padded_binary_size > glb_size_limit - total_size) {
                return Error::fail(Error::Code::ResourceExhausted, "serialized glTF exceeds the GLB size limit");
            }
            total_size += chunk_header_size + padded_binary_size;
        }
    }

    const std::error_code directory_error = ::io::utils::create_parent_directories(path);
    if (directory_error) {
        return Error::fail(Error::Code::Io, "create parent directories for", path, directory_error);
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return Error::fail(Error::Code::Io, "open glTF file for writing", path);
    }

    if (!binary_output) {
        if (!write_all(file, json)) {
            return Error::fail(Error::Code::Io, "write glTF JSON to", path);
        }
    } else {
        const auto put_u32 = [](const std::span<std::uint8_t> bytes, const std::size_t offset, const std::uint32_t value) {
            bytes[offset] = static_cast<std::uint8_t>(value);
            bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
            bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
            bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
        };
        std::array<std::uint8_t, header_size + chunk_header_size> header {};
        put_u32(header, 0, glb_magic);
        put_u32(header, 4, glb_version);
        put_u32(header, 8, static_cast<std::uint32_t>(total_size));
        put_u32(header, 12, static_cast<std::uint32_t>(padded_json_size));
        put_u32(header, 16, json_chunk_magic);
        constexpr std::array<std::uint8_t, 3> json_padding_bytes { 0x20, 0x20, 0x20 };
        constexpr std::array<std::uint8_t, 3> binary_padding_bytes {};

        if (!write_all(file, header)
            || !write_all(file, json)
            || !write_all(file, std::span(json_padding_bytes).first(json_padding))) {
            return Error::fail(Error::Code::Io, "write glTF JSON chunk to", path);
        }
        if (binary_size != 0) {
            std::array<std::uint8_t, chunk_header_size> binary_header {};
            put_u32(binary_header, 0, static_cast<std::uint32_t>(padded_binary_size));
            put_u32(binary_header, 4, binary_chunk_magic);
            const std::span binary_bytes(static_cast<const std::uint8_t*>(data.bin), binary_size);
            if (!write_all(file, binary_header)
                || !write_all(file, binary_bytes)
                || !write_all(file, std::span(binary_padding_bytes).first(binary_padding))) {
                return Error::fail(Error::Code::Io, "write glTF binary chunk to", path);
            }
        }
    }

    file.flush();
    if (!file.good()) {
        return Error::fail(Error::Code::Io, "flush glTF file", path);
    }
    file.close();
    if (file.fail()) {
        return Error::fail(Error::Code::Io, "close glTF file", path);
    }
    return {};
} catch (const std::bad_alloc&) {
    return Error::fail(Error::Code::ResourceExhausted, "allocate glTF writer state");
} catch (const std::length_error&) {
    return Error::fail(Error::Code::ResourceExhausted, "serialized glTF exceeds the supported size");
}

/// Calculates the size of the data of a vector in bytes.
template <typename T>
static size_t vectorsizeof(const typename std::vector<T> &vec) {
    return sizeof(T) * vec.size();
}

static size_t align(std::size_t alignment, size_t offset) noexcept {
    const size_t aligned = (offset - 1u + alignment) & -alignment;
    return aligned;
}

static std::string image_ext_to_mime(std::string_view extension) {
    if (extension.starts_with(".")) {
        extension = extension.substr(1);
    }

    if (extension == "jpg") {
        extension = "jpeg";
    }

    return fmt::format("image/{}", extension);
}
}

Expected<RawMesh> load_raw_from_path(const std::filesystem::path &path) {
    cgltf_options options = {};
    cgltf_data *data = NULL;
    const std::string path_str = path.string();
    const char *path_ptr = path_str.c_str();
    cgltf_result result = cgltf_parse_file(&options, path_ptr, &data);
    if (result != cgltf_result::cgltf_result_success) {
        return Error::propagate(load_cgltf_error(result, path), "parse glTF file");
    }
    RawMesh raw_mesh(data, cgltf_free);

    result = cgltf_load_buffers(&options, raw_mesh.get(), path_ptr);
    if (result != cgltf_result::cgltf_result_success) {
        return Error::propagate(load_cgltf_error(result, path), "load glTF buffers");
    }

    result = cgltf_validate(raw_mesh.get());
    if (result != cgltf_result_success) {
        return Error::propagate(load_cgltf_error(result, path), "validate glTF data");
    }

    return raw_mesh;
}

Expected<SimpleMesh> load_from_raw(const RawMesh &raw, const LoadOptions& /* options */) {
    LOG_TRACE("Loading mesh from gltf data");

    const cgltf_data &data = *raw;

    if (data.meshes_count == 0) {
        return Error::fail(Error::Code::CorruptData, "glTF file contains no mesh");
    }
    if (data.meshes_count > 1) {
        return Error::fail(Error::Code::Unsupported, "loading glTF files with multiple meshes is not supported");
    }
    const cgltf_mesh &mesh = data.meshes[0];

    if (mesh.primitives_count == 0) {
        return Error::fail(Error::Code::CorruptData, "glTF mesh contains no primitive");
    }
    if (mesh.primitives_count > 1) {
        return Error::fail(Error::Code::Unsupported, "loading glTF meshes with multiple primitives is not supported");
    }
    const cgltf_primitive &mesh_primitive = mesh.primitives[0];
    if (mesh_primitive.type != cgltf_primitive_type::cgltf_primitive_type_triangles) {
        LOG_ERROR("mesh has invalid primitive type");
        return Error::fail(Error::Code::Unsupported, "loading non-triangle glTF primitives is not supported");
    }

    // indices
    if (mesh_primitive.indices == nullptr) {
        LOG_ERROR("mesh primitive has no indices");
        return Error::fail(Error::Code::Unsupported, "loading non-indexed glTF primitives is not supported");
    }
    cgltf_accessor &index_accessor = *mesh_primitive.indices;
    std::vector<glm::uvec3> indices;
    if (index_accessor.count % 3 != 0) {
        return Error::fail(Error::Code::CorruptData, "glTF mesh index count is not divisible by three");
    }
    indices.resize(index_accessor.count / 3);
    cgltf_accessor_unpack_indices(
        &index_accessor,
        reinterpret_cast<unsigned int *>(indices.data()),
        sizeof(uint32_t),
        indices.size() * 3);

    // positions
    cgltf_attribute *position_attr = find_attribute_with_type(mesh_primitive.attributes, mesh_primitive.attributes_count, cgltf_attribute_type_position);
    if (position_attr == nullptr) {
        LOG_ERROR("mesh has no position attribute");
        return Error::fail(Error::Code::CorruptData, "glTF mesh contains no position attribute");
    }

    cgltf_accessor &position_accessor = *position_attr->data;
    if (position_accessor.type != cgltf_type_vec3) {
        LOG_WARN("mesh positions are not vec3");
        return Error::fail(Error::Code::CorruptData, "glTF mesh positions are not three-component vectors");
    }
    std::vector<glm::vec3> positions;
    positions.resize(position_accessor.count);
    cgltf_accessor_unpack_floats(&position_accessor, reinterpret_cast<float *>(positions.data()), positions.size() * 3);

    // uvs
    cgltf_attribute *uv_attr = find_attribute_with_type(mesh_primitive.attributes, mesh_primitive.attributes_count, cgltf_attribute_type_texcoord);
    std::vector<glm::vec2> uvs;
    if (uv_attr == nullptr) {
        LOG_WARN("mesh has no uv attribute");
    } else {
        cgltf_accessor &uv_accessor = *uv_attr->data;
        if (uv_accessor.type != cgltf_type_vec2) {
            LOG_WARN("mesh uvss are not vec2");
            return Error::fail(Error::Code::CorruptData, "glTF mesh UVs are not two-component vectors");
        }
        uvs.resize(uv_accessor.count);
        cgltf_accessor_unpack_floats(&uv_accessor, reinterpret_cast<float *>(uvs.data()), uvs.size() * 2);
    }

    glm::dmat4 transform = get_mesh_transform(data, mesh);
    std::vector<glm::dvec3> positionsd;
    positionsd.resize(positions.size());
    for (unsigned int i = 0; i < positions.size(); i++) {
        const glm::dvec4 positiond = glm::dvec4(positions[i], 1);
        const glm::dvec4 transformed = transform * positiond;
        positionsd[i] = glm::dvec3(transformed) / transformed.w;
    }

    std::vector<glm::dvec2> uvsd;
    uvsd.resize(uvs.size());
    for (unsigned int i = 0; i < uvsd.size(); i++) {
        uvsd[i] = glm::dvec2(uvs[i]);
    }

    std::optional<cv::Mat> texture;
    if (mesh_primitive.material != nullptr) {
        auto loaded_texture = load_texture_from_material(*mesh_primitive.material);
        if (!loaded_texture) {
            return Error::propagate(std::move(loaded_texture), "load glTF material texture");
        }
        texture = std::move(*loaded_texture);
    }

    return SimpleMesh(indices, positionsd, uvsd, texture);
}

/// Saves the mesh as a .gltf or .glb file at the given path.
Expected<void> save_to_path(
    const SimpleMesh &terrain_mesh,
    const std::filesystem::path &path,
    const SaveOptions& options) {
    LOG_TRACE("Saving mesh as gltf/glb");

    // ********************* Preprocessing ********************* //

    // Calculate the average vertex position for later normalization.
    const size_t vertex_count = terrain_mesh.positions.size();
    glm::dvec3 average_position(0, 0, 0);
    for (size_t i = 0; i < vertex_count; i++) {
        average_position += terrain_mesh.positions[i] / static_cast<double>(vertex_count);
    }

    // Create vertex data vector from positions and uvs.
    // We also normalize vertex position by extracting their average position and storing the offsets.
    // This is to preserve more of our double accuracy, as gltf cannot store them directly.
    // This helps but does not fully preserve the accuracy.
    std::vector<float> vertices;
    vertices.reserve((vectorsizeof(terrain_mesh.positions) + vectorsizeof(terrain_mesh.uvs)) / sizeof(float));
    glm::vec3 max_position(-std::numeric_limits<float>::infinity());
    glm::vec3 min_position(std::numeric_limits<float>::infinity());
    for (size_t i = 0; i < vertex_count; i++) {
        const glm::vec3 normalized_position = terrain_mesh.positions[i] - average_position;

        vertices.push_back(normalized_position.x);
        vertices.push_back(normalized_position.y);
        vertices.push_back(normalized_position.z);
        if (terrain_mesh.has_uvs()) {
            vertices.push_back(terrain_mesh.uvs[i].x);
            vertices.push_back(terrain_mesh.uvs[i].y);
        }

        max_position = glm::max(max_position, normalized_position);
        min_position = glm::min(min_position, normalized_position);
    }

    // Encode the texture as jpeg data.
    const bool has_texture = terrain_mesh.has_texture();
    std::vector<uint8_t> texture_bytes;
    if (has_texture) {
        try {
            if (!cv::haveImageWriter(options.texture_format)) {
                return Error::fail(Error::Code::Unsupported, "unsupported glTF texture format: " + options.texture_format);
            }
            texture_bytes = mesh::io::write_texture_to_encoded_buffer(terrain_mesh.texture.value(), options.texture_format);
        } catch (const cv::Exception& error) {
            if (error.code == cv::Error::StsNoMem) {
                return Error::fail(Error::Code::ResourceExhausted, "encode glTF texture: out of memory");
            }
            if (error.code == cv::Error::StsUnsupportedFormat || error.code == cv::Error::StsNotImplemented) {
                return Error::fail(Error::Code::Unsupported, "unsupported glTF texture encoding: " + error.msg);
            }
            return Error::fail(Error::Code::InvalidInput, "could not encode glTF texture: " + error.msg);
        }
        if (!terrain_mesh.texture->empty() && texture_bytes.empty()) {
            return Error::fail(Error::Code::InvalidInput, "could not encode glTF texture");
        }
    }

    // Create a single buffer that holds all binary data (indices, vertices, textures)
    // We need to do this because only a single buffer can be written as a binary blob in .glb files.
    const size_t index_data_offset = align(sizeof(glm::uvec3), 0);
    const size_t index_data_byte_count = vectorsizeof(terrain_mesh.triangles);
    const size_t index_data_end = index_data_offset + index_data_byte_count;
    const size_t vertex_data_offset = align(sizeof(glm::vec3), index_data_end);
    const size_t vertex_data_byte_count = vectorsizeof(vertices);
    const size_t vertex_data_end = vertex_data_offset + vertex_data_byte_count;
    const size_t texture_data_offset = align(sizeof(uint8_t), vertex_data_end);
    const size_t texture_data_byte_count = vectorsizeof(texture_bytes);
    const size_t texture_data_end = texture_data_offset + texture_data_byte_count;

    std::vector<uint8_t> buffer_data;
    buffer_data.resize(texture_data_end);
    std::memcpy(buffer_data.data() + index_data_offset, terrain_mesh.triangles.data(), index_data_byte_count);
    std::memcpy(buffer_data.data() + vertex_data_offset, vertices.data(), vertex_data_byte_count);
    if (has_texture) {
        std::memcpy(buffer_data.data() + texture_data_offset, texture_bytes.data(), texture_data_byte_count);
    }

    const bool binary_output = path.extension() == ".glb";

    // ********************* Create GLTF data structure ********************* //

    // Initialize a GLTF data structure
    cgltf_data data = {};
    std::string version = "2.0\0";
    data.asset.version = version.data();
    std::string generator = "alpinite\0";
    data.asset.generator = generator.data();

    std::array<cgltf_buffer, 1> buffers;

    // Create a gltf buffer to hold vertex data, index data and the texture.
    cgltf_buffer &buffer = buffers[0] = {};
    buffer.size = buffer_data.size();
    buffer.data = buffer_data.data();
    std::string buffer_data_encoded;
    if (binary_output) {
        // The binary blob at the end of the file will be used as the contents of the first buffer if it does not have an uri defined.
        data.bin = buffer_data.data();
        data.bin_size = buffer_data.size();
    } else {
        buffer_data_encoded = data_uri_encode(buffer_data.data(), buffer_data.size());
        buffer.uri = buffer_data_encoded.data();
    }

    // Create buffer views for each of types of data in the buffer.
    std::array<cgltf_buffer_view, 3> buffer_views;

    cgltf_buffer_view &index_buffer_view = buffer_views[0] = {};
    index_buffer_view.buffer = &buffer;
    index_buffer_view.offset = index_data_offset;
    index_buffer_view.size = index_data_byte_count;
    index_buffer_view.stride = 0;
    index_buffer_view.type = cgltf_buffer_view_type_indices;

    cgltf_buffer_view &vertex_buffer_view = buffer_views[1] = {};
    vertex_buffer_view.buffer = &buffer;
    vertex_buffer_view.offset = vertex_data_offset;
    vertex_buffer_view.size = vertex_data_byte_count;
    vertex_buffer_view.stride = (terrain_mesh.has_uvs() ? 5 : 3) * sizeof(float);
    vertex_buffer_view.type = cgltf_buffer_view_type_vertices;

    cgltf_buffer_view &texture_buffer_view = buffer_views[2] = {};
    texture_buffer_view.buffer = &buffer;
    texture_buffer_view.offset = texture_data_offset;
    texture_buffer_view.size = texture_data_byte_count;
    texture_buffer_view.stride = 0;

    // Create accessors describing the layout of data in the buffer views.
    std::array<cgltf_accessor, 3> accessors;

    // Create an accessor for indices.
    cgltf_accessor &index_accessor = accessors[0] = {};
    index_accessor.buffer_view = &index_buffer_view;
    index_accessor.type = cgltf_type_scalar;
    index_accessor.component_type = cgltf_component_type_r_32u;
    index_accessor.count = terrain_mesh.face_count() * 3;
    index_accessor.has_min = true;
    index_accessor.min[0] = static_cast<cgltf_float>(0.0);
    index_accessor.has_max = true;
    index_accessor.max[0] = static_cast<cgltf_float>(vertex_count - 1);

    // Create an accessor for vertex positions
    cgltf_accessor &position_accessor = accessors[1] = {};
    position_accessor.buffer_view = &vertex_buffer_view;
    position_accessor.component_type = cgltf_component_type_r_32f;
    position_accessor.type = cgltf_type_vec3;
    position_accessor.offset = 0 * sizeof(float);
    position_accessor.count = vertex_count;
    // We need the min and max as some viewers otherwise refuse to open the file.
    position_accessor.has_min = true;
    position_accessor.has_max = true;
    std::copy(glm::value_ptr(min_position), glm::value_ptr(min_position) + min_position.length(), position_accessor.min);
    std::copy(glm::value_ptr(max_position), glm::value_ptr(max_position) + max_position.length(), position_accessor.max);

    cgltf_accessor &uv_accessor = accessors[2] = {};
    if (terrain_mesh.has_uvs()) {
        // Create an accessor for vertex uvs
        uv_accessor.buffer_view = &vertex_buffer_view;
        uv_accessor.component_type = cgltf_component_type_r_32f;
        uv_accessor.type = cgltf_type_vec2;
        uv_accessor.offset = 3 * sizeof(float);
        uv_accessor.count = vertex_count;
        uv_accessor.has_min = true;
        uv_accessor.has_max = true;
        std::fill(uv_accessor.min, uv_accessor.min + 2, 0);
        std::fill(uv_accessor.max, uv_accessor.max + 2, 1);
    }

    // Create a mesh primitive.
    std::array<cgltf_attribute, 2> primitive_attributes;
    cgltf_attribute &position_attribute = primitive_attributes[0] = {};
    std::string position_attribute_name = "POSITION\0";
    position_attribute.name = position_attribute_name.data();
    position_attribute.type = cgltf_attribute_type::cgltf_attribute_type_position;
    position_attribute.index = 0;
    position_attribute.data = &position_accessor;
    cgltf_attribute &uv_attribute = primitive_attributes[1] = {};
    std::string uv_attribute_name = "TEXCOORD_0\0";
    if (terrain_mesh.has_uvs()) {
        uv_attribute.name = uv_attribute_name.data();
        uv_attribute.type = cgltf_attribute_type::cgltf_attribute_type_texcoord;
        uv_attribute.index = 0;
        uv_attribute.data = &uv_accessor;
    }

    // Create a gltf texture
    std::array<cgltf_image, 1> images;
    cgltf_image &image = images[0] = {};
    image.buffer_view = &texture_buffer_view;
    std::string image_mime_type = image_ext_to_mime(options.texture_format);
    image.mime_type = image_mime_type.data();

    std::array<cgltf_sampler, 1> samplers;
    cgltf_sampler &sampler = samplers[0] = {};
    sampler.min_filter = cgltf_filter_type_linear_mipmap_linear;
    sampler.mag_filter = cgltf_filter_type_linear;
    sampler.wrap_s = cgltf_wrap_mode_clamp_to_edge;
    sampler.wrap_t = cgltf_wrap_mode_clamp_to_edge;

    std::array<cgltf_texture, 1> textures;
    cgltf_texture &texture = textures[0] = {};
    texture.image = &image;
    texture.sampler = &sampler;

    // Create a material
    std::array<cgltf_material, 1> materials;
    cgltf_material &material = materials[0] = {};
    material.has_pbr_metallic_roughness = true;
    material.pbr_metallic_roughness.base_color_factor[0] = 1.0f;
    material.pbr_metallic_roughness.base_color_factor[1] = 0.9f;
    material.pbr_metallic_roughness.base_color_factor[2] = 0.9f;
    material.pbr_metallic_roughness.base_color_factor[3] = 1.0f;
    material.pbr_metallic_roughness.roughness_factor = 1;
    if (has_texture) {
        material.pbr_metallic_roughness.base_color_texture.texture = &texture;
    }
    material.double_sided = true;

    // Build the primitive for the mesh
    std::array<cgltf_primitive, 1> primitives;
    cgltf_primitive &primitive = primitives[0] = {};
    primitive.type = cgltf_primitive_type_triangles;
    primitive.indices = &index_accessor;
    primitive.attributes_count = primitive_attributes.size();
    primitive.attributes = primitive_attributes.data();
    if (!terrain_mesh.has_uvs()) {
        primitive.attributes_count -= 1;
    }
    primitive.material = &material;

    // Build the actual mesh
    std::array<cgltf_mesh, 1> meshes;
    cgltf_mesh &mesh = meshes[0] = {};
    mesh.primitives_count = 1;
    mesh.primitives = &primitive;

    // Create the node hierachy.
    // We create parent nodes to offset the position by the average calculate above.
    // We need multiple parents to ensure that we dont lose our double precision accurary.
    // TODO: dont create this hierachy if the translation is 0
    char *node_name = const_cast<char *>(options.name.data());
    std::array<cgltf_node, 3> nodes;
    cgltf_node &mesh_node = nodes[2] = {};
    mesh_node.name = node_name;
    mesh_node.has_translation = true;
    mesh_node.mesh = &mesh;

    cgltf_node &parent_node = nodes[1] = {};
    parent_node.name = node_name;
    std::array<cgltf_node *, 1> parent_node_children = {&mesh_node};
    parent_node.children_count = parent_node_children.size();
    parent_node.children = parent_node_children.data();
    parent_node.has_translation = true;
    mesh_node.parent = &parent_node;

    cgltf_node &parent_parent_node = nodes[0] = {};
    parent_parent_node.name = node_name;
    std::array<cgltf_node *, 1> parent_parent_node_children = {&parent_node};
    parent_parent_node.children_count = parent_parent_node_children.size();
    parent_parent_node.children = parent_parent_node_children.data();
    parent_parent_node.has_translation = true;
    parent_node.parent = &parent_parent_node;

    const glm::vec3 parent_parent_offset(average_position);
    const glm::dvec3 parent_parent_offset_error = glm::dvec3(parent_parent_offset) - average_position;
    const glm::vec3 parent_offset(-parent_parent_offset_error);
    const glm::dvec3 parent_offset_error = glm::dvec3(parent_offset) + glm::dvec3(parent_parent_offset) - average_position;
    const glm::vec3 mesh_offset(-parent_offset_error);
    std::copy(glm::value_ptr(parent_parent_offset), glm::value_ptr(parent_parent_offset) + parent_parent_offset.length(), parent_parent_node.translation);
    std::copy(glm::value_ptr(parent_offset), glm::value_ptr(parent_offset) + parent_offset.length(), parent_node.translation);
    std::copy(glm::value_ptr(mesh_offset), glm::value_ptr(mesh_offset) + mesh_offset.length(), mesh_node.translation);
    const glm::dvec3 full_error = (glm::dvec3(parent_parent_offset) + glm::dvec3(parent_offset) + glm::dvec3(mesh_offset)) - average_position;
    // DEBUG_ASSERT(glm::length(full_error) == 0);
    if (full_error != glm::dvec3(0)) {
        LOG_ERROR("Float transform trick failed (error: {})", glm::length(full_error));
    }

    // Create a scene
    std::array<cgltf_scene, 1> scenes;
    cgltf_scene &scene = scenes[0] = {};
    std::array<cgltf_node *, 1> scene_nodes = {&parent_parent_node};
    scene.nodes_count = scene_nodes.size();
    scene.nodes = scene_nodes.data();

    // Set up data references
    data.meshes_count = meshes.size();
    data.meshes = meshes.data();
    data.nodes_count = nodes.size();
    data.nodes = nodes.data();
    data.scenes_count = scenes.size();
    data.scenes = scenes.data();
    data.buffers_count = buffers.size();
    data.buffers = buffers.data();
    data.buffer_views_count = buffer_views.size();
    data.buffer_views = buffer_views.data();
    data.accessors_count = accessors.size();
    data.accessors = accessors.data();
    if (!terrain_mesh.has_uvs()) {
        data.accessors_count -= 1;
    }
    data.materials_count = materials.size();
    data.materials = materials.data();
    if (has_texture) {
        data.textures_count = textures.size();
        data.textures = textures.data();
        data.images_count = images.size();
        data.images = images.data();
        data.samplers_count = samplers.size();
        data.samplers = samplers.data();
    } else {
        data.textures_count = 0;
        data.images_count = 0;
        data.samplers_count = 0;
        data.buffer_views_count -= 1;
    }

    // Set up extra metadata
    std::string extras_str;
    const auto &extra_metadata = options.metadata;
    if (!extra_metadata.empty()) {
        std::stringstream extras = {};
        extras << "{";
        for (auto const &[key, val] : extra_metadata) {
            extras << "\n";
            extras << "    ";
            extras << "\"" << key << "\"";
            extras << ": ";
            extras << val;
            extras << ",";
        }
        extras.seekp(-1, std::ios_base::end); // remove last ","
        extras << "\n  }";
        extras << "\0";
        extras_str = extras.str();
        data.extras.data = extras_str.data();
    }

    // ********************* Save the GLTF data to a file ********************* //
    cgltf_options gltf_options = {};
    if (binary_output) {
        gltf_options.type = cgltf_file_type_glb;
    }
    auto serialized = serialize_json(gltf_options, data);
    if (!serialized) {
        return Error::propagate(std::move(serialized), "serialize glTF output");
    }
    auto written = write_serialized(path, *serialized, data, binary_output);
    if (!written) {
        return Error::propagate(std::move(written), "write glTF output");
    }

    return {};
}

Expected<SimpleMesh> load_from_path(const std::filesystem::path &path, const LoadOptions &options) {
    auto raw_mesh = load_raw_from_path(path);
    if (!raw_mesh) {
        return Error::propagate(std::move(raw_mesh), "load raw glTF data");
    }
    auto mesh = load_from_raw(*raw_mesh, options);
    if (!mesh) {
        return Error::propagate(std::move(mesh), "decode glTF mesh \"" + path.string() + "\"");
    }
    return std::move(*mesh);
}

} // namespace mesh::io::gltf

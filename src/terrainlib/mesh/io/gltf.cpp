#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
// #define CGLTF_VALIDATE_ENABLE_ASSERTS
#include <cgltf_write.h>
#undef CGLTF_IMPLEMENTATION
#undef CGLTF_WRITE_IMPLEMENTATION

#include "log.h"
#include "mesh/io/utils.h"
#include "mesh/io/gltf.h"
#include "mesh/io/texture.h"

using namespace mesh::io::utils;

namespace mesh {
namespace io {
namespace gltf {

namespace {
const int GL_NEAREST = 0x2600;
const int GL_LINEAR = 0x2601;
const int GL_NEAREST_MIPMAP_NEAREST = 0x2700;
const int GL_LINEAR_MIPMAP_NEAREST = 0x2701;
const int GL_NEAREST_MIPMAP_LINEAR = 0x2702;
const int GL_LINEAR_MIPMAP_LINEAR = 0x2703;
const int GL_TEXTURE_MAG_FILTER = 0x2800;
const int GL_TEXTURE_MIN_FILTER = 0x2801;
const int GL_REPEAT = 0x2901;
const int GL_CLAMP_TO_EDGE = 0x812F;
const int GL_MIRRORED_REPEAT = 0x8370;

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
        transform *= parent_transform;
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

std::optional<cv::Mat> load_texture_from_material(const cgltf_material &material) {
    if (!material.has_pbr_metallic_roughness || material.pbr_metallic_roughness.base_color_texture.texture == nullptr) {
        LOG_WARN("mesh material has no texture");
        return std::nullopt;
    }

    cgltf_texture &albedo_texture = *material.pbr_metallic_roughness.base_color_texture.texture;
    cgltf_image &albedo_image = *albedo_texture.image;

    const std::span<const uint8_t> raw_texture{cgltf_buffer_view_data(albedo_image.buffer_view), albedo_image.buffer_view->size};
    cv::Mat texture = mesh::io::texture::read_texture_from_encoded_bytes(raw_texture);
    return texture;
}

#define GET_OR_INVALID_FORMAT(var, opt)                              \
    do {                                                             \
        if (!(opt).has_value()) {                                    \
            return tl::unexpected(LoadMeshErrorKind::InvalidFormat); \
        } else {                                                     \
            var = opt.value();                                       \
        }                                                            \
    } while (false)

template <typename T>
static std::optional<std::reference_wrapper<const T>> get_single_element(const char *name, cgltf_size count, T const *items) {
    if (count == 0) {
        LOG_ERROR("file contains no {}", name);
        return std::nullopt;
    }
    if (count > 1) {
        LOG_WARN("file contains more than one {}", name);
        return std::nullopt;
    }

    const T &ref = items[0];

    return ref;
}

LoadMeshError map_cgltf_error(cgltf_result result) {
    switch (result) {
    case cgltf_result::cgltf_result_data_too_short:
    case cgltf_result::cgltf_result_unknown_format:
    case cgltf_result::cgltf_result_invalid_json:
    case cgltf_result::cgltf_result_invalid_gltf:
    case cgltf_result::cgltf_result_legacy_gltf:
        return LoadMeshErrorKind::InvalidFormat;
    case cgltf_result::cgltf_result_file_not_found:
    case cgltf_result::cgltf_result_io_error:
        return LoadMeshErrorKind::FileNotFound;
    case cgltf_result::cgltf_result_out_of_memory:
        return LoadMeshErrorKind::OutOfMemory;
    default:
        assert(false);
        break;
    }
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

    return fmt::format("image/{}", extension);
}
}

tl::expected<RawMesh, cgltf_result> load_raw_from_path(const std::filesystem::path &path) {
    cgltf_options options = {};
    cgltf_data *data = NULL;
    const std::string path_str = path.string();
    const char *path_ptr = path_str.c_str();
    cgltf_result result = cgltf_parse_file(&options, path_ptr, &data);
    if (result != cgltf_result::cgltf_result_success) {
        return tl::unexpected(result);
    }
    result = cgltf_load_buffers(&options, data, path_ptr);
    if (result != cgltf_result::cgltf_result_success) {
        return tl::unexpected(result);
    }
    return RawMesh(data, cgltf_free);
}

tl::expected<SimpleMesh, LoadMeshError> load_mesh_from_raw(const RawMesh &raw, const LoadOptions& options) {
    LOG_TRACE("Loading mesh from gltf data");

    const cgltf_data &data = *raw;

    const auto mesh_opt = get_single_element("mesh", data.meshes_count, data.meshes);
    if (!mesh_opt.has_value()) {
        return tl::unexpected(LoadMeshErrorKind::InvalidFormat);
    }
    const cgltf_mesh &mesh = mesh_opt.value();

    const auto mesh_primitive_opt = get_single_element("mesh primitive", mesh.primitives_count, mesh.primitives);
    if (!mesh_primitive_opt.has_value()) {
        return tl::unexpected(LoadMeshErrorKind::InvalidFormat);
    }
    const cgltf_primitive &mesh_primitive = mesh_primitive_opt.value();
    if (mesh_primitive.type != cgltf_primitive_type::cgltf_primitive_type_triangles) {
        LOG_ERROR("mesh has invalid primitive type");
        return tl::unexpected(LoadMeshErrorKind::InvalidFormat);
    }

    // indices
    cgltf_accessor &index_accessor = *mesh_primitive.indices;
    std::vector<glm::uvec3> indices;
    indices.resize(index_accessor.count / 3);
    cgltf_accessor_unpack_indices(&index_accessor, reinterpret_cast<unsigned int *>(indices.data()), cgltf_component_size(index_accessor.component_type), indices.size() * 3);

    // positions
    cgltf_attribute *position_attr = find_attribute_with_type(mesh_primitive.attributes, mesh_primitive.attributes_count, cgltf_attribute_type_position);
    if (position_attr == nullptr) {
        LOG_ERROR("mesh has no position attribute");
        return tl::unexpected(LoadMeshErrorKind::InvalidFormat);
    }

    cgltf_accessor &position_accessor = *position_attr->data;
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

    std::optional<cv::Mat> texture = load_texture_from_material(*mesh_primitive.material);

    return SimpleMesh(indices, positionsd, uvsd, texture);
}

/// Saves the mesh as a .gltf or .glb file at the given path.
tl::expected<void, SaveMeshError> save_to_path(
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
        texture_bytes = mesh::io::texture::write_texture_to_encoded_buffer(terrain_mesh.texture.value(), options.texture_format);
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
    std::memcpy(buffer_data.data() + texture_data_offset, texture_bytes.data(), texture_data_byte_count);

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
    sampler.min_filter = GL_LINEAR_MIPMAP_LINEAR;
    sampler.mag_filter = GL_LINEAR;
    sampler.wrap_s = GL_CLAMP_TO_EDGE;
    sampler.wrap_t = GL_CLAMP_TO_EDGE;

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

    cgltf_node &parent_parent_node = nodes[0] = {};
    parent_parent_node.name = node_name;
    std::array<cgltf_node *, 1> parent_parent_node_children = {&parent_node};
    parent_parent_node.children_count = parent_parent_node_children.size();
    parent_parent_node.children = parent_parent_node_children.data();
    parent_parent_node.has_translation = true;

    const glm::vec3 parent_parent_offset(average_position);
    const glm::dvec3 parent_parent_offset_error = glm::dvec3(parent_parent_offset) - average_position;
    const glm::vec3 parent_offset(-parent_parent_offset_error);
    const glm::dvec3 parent_offset_error = glm::dvec3(parent_offset) + glm::dvec3(parent_parent_offset) - average_position;
    const glm::vec3 mesh_offset(-parent_offset_error);
    std::copy(glm::value_ptr(parent_parent_offset), glm::value_ptr(parent_parent_offset) + parent_parent_offset.length(), parent_parent_node.translation);
    std::copy(glm::value_ptr(parent_offset), glm::value_ptr(parent_offset) + parent_offset.length(), parent_node.translation);
    std::copy(glm::value_ptr(mesh_offset), glm::value_ptr(mesh_offset) + mesh_offset.length(), mesh_node.translation);
    const glm::dvec3 full_error = (glm::dvec3(parent_parent_offset) + glm::dvec3(parent_offset) + glm::dvec3(mesh_offset)) - average_position;
    // assert(glm::length(full_error) == 0);
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
    create_parent_directories(path);
    cgltf_options gltf_options;
    if (binary_output) {
        gltf_options.type = cgltf_file_type_glb;
    }
    if (cgltf_write_file(&gltf_options, path.c_str(), &data) != cgltf_result_success) {
        throw std::runtime_error("Failed to save GLTF file");
    }

    return {};
}

tl::expected<SimpleMesh, LoadMeshError> load_from_path(const std::filesystem::path &path, const LoadOptions &options) {
    tl::expected<RawMesh, cgltf_result> raw_mesh = load_raw_from_path(path);
    if (!raw_mesh) {
        return tl::unexpected(map_cgltf_error(raw_mesh.error()));
    }
    return load_mesh_from_raw(*raw_mesh, options);
}

} // namespace gltf
} // namespace io
} // namespace mesh

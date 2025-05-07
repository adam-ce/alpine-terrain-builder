#pragma once

#include <filesystem>
#include <optional>
#include <vector>
#include <span>

#include <cgltf_write.h>
#include <fmt/core.h>
#include <glm/glm.hpp>
#include <tl/expected.hpp>

#include "terrain_mesh.h"
#include "log.h"
#include "raw_gltf.h"

namespace io {

struct LoadOptions {
};

enum class LoadMeshErrorKind {
    UnsupportedFormat,
    FileNotFound,
    InvalidFormat,
    OutOfMemory
};

class LoadMeshError {
public:
    LoadMeshError() = default;
    constexpr LoadMeshError(LoadMeshErrorKind kind)
        : kind(kind) {}

    operator LoadMeshErrorKind() const {
        return this->kind;
    }
    constexpr bool operator==(LoadMeshError other) const {
        return this->kind == other.kind;
    }
    constexpr bool operator!=(LoadMeshError other) const {
        return this->kind != other.kind;
    }

    std::string description() const {
        switch (kind) {
        case LoadMeshErrorKind::UnsupportedFormat:
            return "format not supported";
        case LoadMeshErrorKind::FileNotFound:
            return "file not found";
        case LoadMeshErrorKind::InvalidFormat:
            return "invalid file format";
        case LoadMeshErrorKind::OutOfMemory:
            return "out of memory";
        default:
            return "undefined error";
        }
    }

private:
    LoadMeshErrorKind kind;
};

tl::expected<TerrainMesh, LoadMeshError> load_mesh_from_raw(const RawGltfMesh &raw, const LoadOptions = {});
tl::expected<TerrainMesh, LoadMeshError> load_mesh_from_path(const std::filesystem::path &path, const LoadOptions = {});

struct SaveOptions {
    std::string texture_format = ".jpeg";
    std::string name = "Tile";
    std::unordered_map<std::string, std::string> metadata = {};
};

enum class SaveMeshErrorKind {
    UnsupportedFormat,
    OpenFile,
    WriteFile,
    OutOfMemory
};

class SaveMeshError {
public:
    SaveMeshError() = default;
    constexpr SaveMeshError(SaveMeshErrorKind kind)
        : kind(kind) {}

    operator SaveMeshErrorKind() const {
        return this->kind;
    }
    constexpr bool operator==(SaveMeshError other) const {
        return this->kind == other.kind;
    }
    constexpr bool operator!=(SaveMeshError other) const {
        return this->kind != other.kind;
    }

    std::string description() const {
        switch (kind) {
        case SaveMeshErrorKind::UnsupportedFormat:
            return "format not supported";
        case SaveMeshErrorKind::OpenFile:
            return "failed to open output file";
        case SaveMeshErrorKind::WriteFile:
            return "failed to write to output file";
        case SaveMeshErrorKind::OutOfMemory:
            return "out of memory";
        default:
            return "undefined error";
        }
    }

private:
    SaveMeshErrorKind kind;
};

tl::expected<void, SaveMeshError> save_mesh_to_path(const std::filesystem::path &path, const TerrainMesh &mesh, const SaveOptions options = {});

cv::Mat read_texture_from_encoded_bytes(std::span<const uint8_t> buffer);
void write_texture_to_encoded_buffer(const cv::Mat &image, std::vector<uint8_t> &buffer, const std::string extension = ".png");
std::vector<uint8_t> write_texture_to_encoded_buffer(const cv::Mat &image, const std::string extension = ".png");

}

// custom serialization
namespace zpp::bits {
template<typename T>
constexpr auto serialize(auto &archive, glm::tvec2<T> &v) {
    return archive(v.x, v.y);
}

template<typename T>
constexpr auto serialize(auto &archive, const glm::tvec2<T> &v) {
    return archive(v.x, v.y);
}

template <typename T>
constexpr auto serialize(auto &archive, glm::tvec3<T> &v) {
    return archive(v.x, v.y, v.z);
}

template <typename T>
constexpr auto serialize(auto &archive, const glm::tvec3<T> &v) {
    return archive(v.x, v.y, v.z);
}

auto serialize(auto &archive, cv::Mat &v) {
    std::vector<uint8_t> buf;
    auto result = archive(buf);
    v = io::read_texture_from_encoded_bytes(buf);
    return result;
}

auto serialize(auto &archive, const cv::Mat &v) {
    return archive(io::write_texture_to_encoded_buffer(v));
}
} // namespace zpp::bits


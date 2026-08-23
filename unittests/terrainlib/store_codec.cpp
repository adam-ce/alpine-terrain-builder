#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "io/bytes.h"
#include "mesh/codec/Gltf.h"
#include "mesh/codec/SfMesh.h"
#include "store/Codec.h"

namespace {

class TemporaryDirectory {
public:
    explicit TemporaryDirectory(const std::string_view label)
    {
        static std::atomic_uint64_t counter = 0;
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path()
            / ("atb-store-codec-" + std::string(label) + "-" + std::to_string(timestamp) + "-" + std::to_string(counter++));
        REQUIRE(std::filesystem::create_directories(m_path));
    }
    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }
    const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
};

class MultiFileCodec final : public store::Codec<int> {
public:
    std::vector<std::filesystem::path> paths(const store::NodePath& node_path) const override
    {
        std::filesystem::path data = node_path.path();
        std::filesystem::path metadata = node_path.path();
        data += ".data";
        metadata += ".metadata";
        return { data, metadata };
    }
};

class WriteOnlyCodec final : public store::Codec<int> {
public:
    std::vector<std::filesystem::path> paths(const store::NodePath& node_path) const override
    {
        std::filesystem::path path = node_path.path();
        path += ".out";
        return { path };
    }
    std::expected<void, store::CodecError> write(const store::NodePath&, const int&) const override
    {
        ++writes;
        return {};
    }

    mutable std::atomic_uint32_t writes = 0;
};

mesh::Simple triangle_mesh(const double offset)
{
    return mesh::Simple({ { 0, 1, 2 } }, { { offset, 0.0, 0.0 }, { offset + 1.0, 0.0, 0.0 }, { offset, 1.0, 0.0 } });
}

template <typename Codec>
void check_mesh_codec_reentrancy(const Codec& codec, const std::filesystem::path& base)
{
    std::vector<std::future<bool>> writes;
    for (int index = 0; index < 4; ++index) {
        writes.push_back(std::async(std::launch::async,
            [&codec, base, index] { return codec.write(store::NodePath(base / std::to_string(index) / "node"), triangle_mesh(index)).has_value(); }));
    }
    for (auto& write : writes) {
        REQUIRE(write.get());
    }

    std::vector<std::future<bool>> reads;
    for (int index = 0; index < 4; ++index) {
        reads.push_back(std::async(std::launch::async, [&codec, base, index] {
            const auto result = codec.read(store::NodePath(base / std::to_string(index) / "node"));
            return result.has_value() && result->face_count() == 1;
        }));
    }
    for (auto& read : reads) {
        REQUIRE(read.get());
    }
}

} // namespace

TEST_CASE("runtime codec defaults report unsupported operations", "[store][codec]")
{
    MultiFileCodec codec;
    const store::NodePath path("12/34/56/78");
    CHECK(codec.paths(path)
        == std::vector<std::filesystem::path> {
            "12/34/56/78.data",
            "12/34/56/78.metadata",
        });

    const auto read = codec.read(path);
    REQUIRE_FALSE(read.has_value());
    CHECK(read.error().operation == store::CodecOperation::Read);
    CHECK(read.error().category == store::CodecErrorCategory::UnsupportedOperation);

    const auto write = codec.write(path, 42);
    REQUIRE_FALSE(write.has_value());
    CHECK(write.error().operation == store::CodecOperation::Write);
    CHECK(write.error().category == store::CodecErrorCategory::UnsupportedOperation);
}

TEST_CASE("runtime write-only codec remains explicitly unreadable", "[store][codec]")
{
    WriteOnlyCodec codec;
    REQUIRE(codec.write(store::NodePath("node"), 42).has_value());
    CHECK(codec.writes == 1);
    const auto read = codec.read(store::NodePath("node"));
    REQUIRE_FALSE(read.has_value());
    CHECK(read.error().category == store::CodecErrorCategory::UnsupportedOperation);
}

TEST_CASE("runtime mesh codecs are reentrant", "[store][codec]")
{
    TemporaryDirectory directory("mesh-reentrant");

    SECTION("SF mesh") { check_mesh_codec_reentrancy(mesh::codec::SfMesh {}, directory.path() / "sfmesh"); }
    SECTION("binary glTF") { check_mesh_codec_reentrancy(mesh::codec::Gltf(mesh::codec::GltfContainer::Binary), directory.path() / "glb"); }
    SECTION("JSON glTF") { check_mesh_codec_reentrancy(mesh::codec::Gltf(mesh::codec::GltfContainer::Json), directory.path() / "gltf"); }
}

TEST_CASE("runtime glTF codec converts writer exceptions", "[store][codec]")
{
    TemporaryDirectory directory("gltf-error");
    mesh::codec::Gltf codec(mesh::codec::GltfContainer::Binary);
    const store::NodePath node_path(directory.path() / "blocked");
    REQUIRE(std::filesystem::create_directory(codec.paths(node_path).front()));

    const auto result = codec.write(node_path, triangle_mesh(0.0));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().operation == store::CodecOperation::Write);
    CHECK(result.error().category == store::CodecErrorCategory::Domain);
}

#include <array>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "io/bytes.h"
#include "mesh/SimpleMesh.h"
#include "mesh/codec/SfMeshFormat.h"
#include "mesh/codec/from_extension.h"
#include "mesh/storage.h"
#include "octree/storage/IndexFile.h"
#include "octree/store_layout/Mappings.h"
#include "../temporary_directory.h"

namespace {

using test::TemporaryDirectory;

mesh::Simple triangle_mesh(const double offset)
{
    return mesh::Simple({ { 0, 1, 2 } }, { { offset, 0.0, 0.0 }, { offset + 1.0, 0.0, 0.0 }, { offset, 1.0, 0.0 } });
}

mesh::storage::IndexedStorage make_storage(
    const std::filesystem::path& path, const store::path_layout::Mapping<octree::Id> mapping, const std::string& extension)
{
    auto codec = mesh::codec::from_extension(extension);
    REQUIRE(codec.has_value());
    return mesh::storage::IndexedStorage(
        store::RawStorage<octree::StoreTraits, mesh::Simple>(store::path_layout::Resolver<octree::Id>(path, mapping), std::move(codec.value())),
        store::Index<octree::StoreTraits> {},
        mesh::storage::Storage::Persistence {
            octree::storage::index_format(),
            path / "octree.storeindex",
            std::string(mapping.id),
            std::string(mesh::storage::payload_class),
            extension,
        });
}

} // namespace

TEST_CASE("octree layouts round-trip boundary IDs")
{
    const std::vector ids {
        octree::Id::root(),
        octree::Id(octree::Id::max_level(), octree::Id::Index { 0 }),
        octree::Id(octree::Id::max_level(), octree::Id::max_index_on_level(octree::Id::max_level())),
    };

    const auto flat = octree::store_layout::flat();
    const auto coordinates = octree::store_layout::level_and_coordinate_directories();
    for (const octree::Id id : ids) {
        const auto flat_path = flat.key_to_node_path(id);
        CHECK(flat.node_path_to_key(flat_path) == id);

        const auto coordinate_path = coordinates.key_to_node_path(id);
        CHECK(coordinates.node_path_to_key(coordinate_path) == id);
    }
}

TEST_CASE("versioned octree and SF payload validation is free-standing")
{
    const octree::storage::StoreIndex invalid_index { { {
        .level = std::numeric_limits<std::uint8_t>::max(),
        .index = 0,
        .status = static_cast<std::uint8_t>(store::NodeStatus::Leaf),
    } } };
    CHECK_FALSE(octree::storage::decode_index(invalid_index).has_value());

    const mesh::sf::Payload oversized_mesh {
        .vertex_count = std::numeric_limits<std::uint32_t>::max(),
        .face_count = 0,
        .triangles = {},
        .positions = { 0 },
        .uvs = {},
        .texture = {},
    };
    CHECK_FALSE(mesh::sf::validate(oversized_mesh).has_value());
}

TEST_CASE("storage hard-links matching payload formats")
{
    TemporaryDirectory source_directory("hard-link-source");
    TemporaryDirectory target_directory("hard-link-target");
    auto source = make_storage(source_directory.path(), octree::store_layout::flat(), ".sfmesh");
    auto target = make_storage(target_directory.path(), octree::store_layout::flat(), ".sfmesh");

    const octree::Id root = octree::Id::root();
    REQUIRE(source.save(root, triangle_mesh(0.0)).has_value());
    REQUIRE(target.copy_from(root, source).has_value());
    CHECK(std::filesystem::equivalent(source.path_for(root).value(), target.path_for(root).value()));
    REQUIRE(source.save_index().has_value());
    REQUIRE(target.save_index().has_value());
}

TEST_CASE("storage re-encodes differing payload formats")
{
    TemporaryDirectory source_directory("reencode-source");
    TemporaryDirectory target_directory("reencode-target");
    auto source = make_storage(source_directory.path(), octree::store_layout::flat(), ".sfmesh");
    auto target = make_storage(target_directory.path(), octree::store_layout::flat(), ".glb");

    const octree::Id root = octree::Id::root();
    REQUIRE(source.save(root, triangle_mesh(4.0)).has_value());
    REQUIRE(target.copy_from(root, source).has_value());
    CHECK_FALSE(std::filesystem::equivalent(source.path_for(root).value(), target.path_for(root).value()));
    const auto loaded = target.load(root);
    REQUIRE(loaded.has_value());
    CHECK(loaded->face_count() == 1);
    REQUIRE(source.save_index().has_value());
    REQUIRE(target.save_index().has_value());
}

TEST_CASE("storage supports enabled overwrites")
{
    TemporaryDirectory directory("overwrite");
    auto storage = make_storage(directory.path(), octree::store_layout::flat(), ".sfmesh");
    storage.settings().allow_overwrite = true;

    const octree::Id root = octree::Id::root();
    REQUIRE(storage.save(root, triangle_mesh(1.0)).has_value());
    REQUIRE(storage.save(root, triangle_mesh(9.0)).has_value());
    const auto loaded = storage.load(root);
    REQUIRE(loaded.has_value());
    REQUIRE_FALSE(loaded->positions.empty());
    CHECK(loaded->positions.front().x == 9.0);
    REQUIRE(storage.save_index().has_value());
}

TEST_CASE("folder opening persists and reopens metadata-driven storage")
{
    TemporaryDirectory directory("folder-open");
    mesh::storage::OpenOptions options;
    options.default_mapping = octree::store_layout::flat();
    options.preferred_extension = ".sfmesh";

    {
        auto storage_result = mesh::storage::open_folder(directory.path(), std::move(options));
        REQUIRE(storage_result.has_value());
        auto storage = std::move(storage_result.value());
        CHECK(storage.is_indexed());
        REQUIRE(storage.save(octree::Id::root(), triangle_mesh(2.0)).has_value());
        REQUIRE(storage.save_index().has_value());
        CHECK(std::filesystem::is_regular_file(directory.path() / "octree.storeindex"));
        CHECK(std::filesystem::is_regular_file(directory.path() / "octree.storemeta"));
    }

    {
        auto storage_result = mesh::storage::open_folder_indexed(directory.path());
        REQUIRE(storage_result.has_value());
        const auto storage = std::move(storage_result.value());
        CHECK(storage.is_indexed());
        CHECK(storage.has(octree::Id::root()).value());
        CHECK(std::filesystem::is_regular_file(directory.path() / "octree.storeindex"));
    }

    const auto reopened = mesh::storage::open_index(directory.path() / "octree.storeindex");
    REQUIRE(reopened.has_value());
    CHECK(reopened->has(octree::Id::root()).value());

    const auto metadata = octree::storage::read_store_metadata(directory.path());
    REQUIRE(metadata.has_value());
    CHECK(metadata->layout_id == "flat");
    CHECK(metadata->payload_class == mesh::storage::payload_class);
    CHECK(metadata->codec_selector == ".sfmesh");
}

TEST_CASE("mesh codec resolver dispatches every supported selector", "[store][open]")
{
    for (const std::string extension : { ".sfmesh", ".glb", ".gltf" }) {
        const auto codec = mesh::codec::from_extension(extension);
        REQUIRE(codec.has_value());
        const auto paths = codec.value()->paths("node");
        REQUIRE_FALSE(paths.empty());
        CHECK(paths.front().extension() == extension);
    }

    const auto unknown = mesh::codec::from_extension(".unknown");
    REQUIRE_FALSE(unknown.has_value());
    CHECK(unknown.error().category == store::CodecErrorCategory::UnsupportedCodec);
    CHECK(unknown.error().operation == store::CodecOperation::Resolve);
}

TEST_CASE("octree opening rejects a mismatched metadata payload class", "[store][open]")
{
    TemporaryDirectory directory("payload-class");
    const std::filesystem::path index_path = directory.path() / "octree.storeindex";
    const store::IndexMetadata<octree::StoreTraits> index_file {
        .index = {},
        .layout_id = "flat",
        .payload_class = "dag.ClusterBatch",
        .codec_selector = ".sfmesh",
    };
    REQUIRE(octree::storage::write_index_file(index_path, index_file).has_value());

    const auto result = mesh::storage::open_index(index_path);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(std::holds_alternative<store::CodecError>(result.error()));
    CHECK(std::get<store::CodecError>(result.error()).category == store::CodecErrorCategory::UnsupportedCodec);
}

TEST_CASE("octree opening retains index failures without directory fallback", "[store][open]")
{
    TemporaryDirectory directory("open-errors");
    const std::filesystem::path index_path = directory.path() / "octree.storeindex";

    store::IndexMetadata<octree::StoreTraits> index_file {
        .index = {},
        .layout_id = "unknown-layout",
        .payload_class = std::string(mesh::storage::payload_class),
        .codec_selector = ".sfmesh",
    };
    REQUIRE(octree::storage::write_index_file(index_path, index_file).has_value());

    const auto unknown_layout = mesh::storage::open_folder_indexed(directory.path());
    REQUIRE_FALSE(unknown_layout.has_value());
    REQUIRE(std::holds_alternative<store::UnknownLayout>(unknown_layout.error()));
    CHECK(std::get<store::UnknownLayout>(unknown_layout.error()).id == "unknown-layout");

    index_file.layout_id = "flat";
    index_file.codec_selector = ".unknown";
    REQUIRE(octree::storage::write_index_file(index_path, index_file).has_value());
    const auto unknown_codec = mesh::storage::open_folder_indexed(directory.path());
    REQUIRE_FALSE(unknown_codec.has_value());
    REQUIRE(std::holds_alternative<store::CodecError>(unknown_codec.error()));
    CHECK(std::get<store::CodecError>(unknown_codec.error()).category == store::CodecErrorCategory::UnsupportedCodec);

    const std::array<uint8_t, 3> malformed_bytes { 0xff, 0x00, 0x01 };
    REQUIRE(io::write_bytes_to_path(malformed_bytes, index_path).has_value());
    const auto malformed = mesh::storage::open_folder_indexed(directory.path());
    REQUIRE_FALSE(malformed.has_value());
    REQUIRE(std::holds_alternative<store::IndexFormatError>(malformed.error()));
    CHECK(std::get<store::IndexFormatError>(malformed.error()).category == store::IndexFormatErrorCategory::Malformed);

    const auto independent_metadata = octree::storage::read_store_metadata(directory.path());
    REQUIRE(independent_metadata.has_value());
    CHECK(independent_metadata->codec_selector == ".unknown");
}

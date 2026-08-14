#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "io/serialize.h"
#include "io/bytes.h"
#include "mesh/SimpleMesh.h"
#include "octree/disk/IndexFile.h"
#include "mesh/codec/from_extension.h"
#include "mesh/storage.h"
#include "octree/store_layout/Mappings.h"
#include "octree/storage/IndexFile.h"
#include "octree/storage/open.h"

namespace {

class TemporaryDirectory {
public:
    explicit TemporaryDirectory(const std::string_view label) {
        static std::atomic_uint64_t counter = 0;
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        _path = std::filesystem::temp_directory_path()
            / ("atb-raster-store-" + std::string(label) + "-"
               + std::to_string(timestamp) + "-" + std::to_string(counter++));
        REQUIRE(std::filesystem::create_directories(_path));
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(_path, error);
    }

    const std::filesystem::path &path() const {
        return _path;
    }

private:
    std::filesystem::path _path;
};

mesh::Simple triangle_mesh(const double offset) {
    return mesh::Simple(
        {{0, 1, 2}},
        {{offset, 0.0, 0.0}, {offset + 1.0, 0.0, 0.0}, {offset, 1.0, 0.0}});
}

mesh::storage::IndexedStorage make_storage(
    const std::filesystem::path &path,
    const store::PathMapping<octree::Id> mapping,
    const std::string &extension) {
    auto codec = mesh::codec::from_extension(extension);
    REQUIRE(codec.has_value());
    return mesh::storage::IndexedStorage(
        store::RawStorage<octree::StoreTraits, mesh::Simple>(
            store::Layout<octree::Id>(path, mapping),
            std::move(codec.value())),
        store::Index<octree::StoreTraits>{},
        mesh::storage::Storage::Persistence{
            octree::storage::index_format(),
            path / "terrain.index",
            std::string(mapping.id),
            extension,
        });
}

} // namespace

TEST_CASE("octree layouts round-trip boundary IDs") {
    const std::vector ids{
        octree::Id::root(),
        octree::Id(octree::Id::max_level(), octree::Id::Index{0}),
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

TEST_CASE("storage hard-links matching payload formats") {
    TemporaryDirectory source_directory("hard-link-source");
    TemporaryDirectory target_directory("hard-link-target");
    auto source = make_storage(
        source_directory.path(),
        octree::store_layout::flat(),
        ".terrain");
    auto target = make_storage(
        target_directory.path(),
        octree::store_layout::flat(),
        ".terrain");

    const octree::Id root = octree::Id::root();
    REQUIRE(source.save(root, triangle_mesh(0.0)).has_value());
    REQUIRE(target.copy_from(root, source).has_value());
    CHECK(std::filesystem::equivalent(source.path_for(root).value(), target.path_for(root).value()));
    REQUIRE(source.save_index().has_value());
    REQUIRE(target.save_index().has_value());
}

TEST_CASE("storage re-encodes differing payload formats") {
    TemporaryDirectory source_directory("reencode-source");
    TemporaryDirectory target_directory("reencode-target");
    auto source = make_storage(
        source_directory.path(),
        octree::store_layout::flat(),
        ".terrain");
    auto target = make_storage(
        target_directory.path(),
        octree::store_layout::flat(),
        ".glb");

    const octree::Id root = octree::Id::root();
    REQUIRE(source.save(root, triangle_mesh(4.0)).has_value());
    REQUIRE(target.copy_from(root, source).has_value());
    CHECK_FALSE(std::filesystem::equivalent(
        source.path_for(root).value(),
        target.path_for(root).value()));
    const auto loaded = target.load(root);
    REQUIRE(loaded.has_value());
    CHECK(loaded->face_count() == 1);
    REQUIRE(source.save_index().has_value());
    REQUIRE(target.save_index().has_value());
}

TEST_CASE("storage supports enabled overwrites") {
    TemporaryDirectory directory("overwrite");
    auto storage = make_storage(
        directory.path(),
        octree::store_layout::flat(),
        ".terrain");
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

TEST_CASE("folder opening scans payloads and creates an index") {
    TemporaryDirectory directory("folder-open");
    octree::OpenOptions options;
    options.default_mapping = octree::store_layout::flat();
    options.preferred_extension = ".terrain";

    {
        auto storage_result = octree::open_folder(directory.path(), false, std::move(options));
        REQUIRE(storage_result.has_value());
        auto storage = std::move(storage_result.value());
        CHECK_FALSE(storage.is_indexed());
        REQUIRE(storage.save(octree::Id::root(), triangle_mesh(2.0)).has_value());
        CHECK_FALSE(std::filesystem::exists(directory.path() / "terrain.index"));
    }

    {
        auto storage_result = octree::open_folder_indexed(directory.path());
        REQUIRE(storage_result.has_value());
        const auto storage = std::move(storage_result.value());
        CHECK(storage.is_indexed());
        CHECK(storage.has(octree::Id::root()).value());
        CHECK(std::filesystem::is_regular_file(directory.path() / "terrain.index"));
    }

    const auto reopened = octree::open_index(directory.path() / "terrain.index");
    REQUIRE(reopened.has_value());
    CHECK(reopened->has(octree::Id::root()).value());
}

TEST_CASE("mesh codec resolver dispatches every supported legacy selector", "[store][open]") {
    for (const std::string extension : {".terrain", ".glb", ".gltf"}) {
        const auto codec = mesh::codec::from_extension(extension);
        REQUIRE(codec.has_value());
        const auto paths = codec.value()->paths(store::NodePath("node"));
        REQUIRE_FALSE(paths.empty());
        CHECK(paths.front().extension() == extension);
    }

    const auto unknown = mesh::codec::from_extension(".unknown");
    REQUIRE_FALSE(unknown.has_value());
    CHECK(unknown.error().category == store::CodecErrorCategory::UnsupportedCodec);
    CHECK(unknown.error().operation == store::CodecOperation::Resolve);
}

TEST_CASE("octree opening retains index failures without directory fallback", "[store][open]") {
    TemporaryDirectory directory("open-errors");
    const std::filesystem::path index_path = directory.path() / "terrain.index";

    octree::disk::v1::IndexFile index_file;
    index_file.layout_strategy_id = "unknown-layout";
    index_file.preferred_extension = ".terrain";
    REQUIRE(io::write_to_path(index_file, index_path).has_value());

    const auto unknown_layout = octree::open_folder_indexed(directory.path());
    REQUIRE_FALSE(unknown_layout.has_value());
    REQUIRE(std::holds_alternative<store::UnknownLayout>(unknown_layout.error()));
    CHECK(std::get<store::UnknownLayout>(unknown_layout.error()).id == "unknown-layout");

    index_file.layout_strategy_id = "flat";
    index_file.preferred_extension = ".unknown";
    REQUIRE(io::write_to_path(index_file, index_path).has_value());
    const auto unknown_codec = octree::open_folder_indexed(directory.path());
    REQUIRE_FALSE(unknown_codec.has_value());
    REQUIRE(std::holds_alternative<store::CodecError>(unknown_codec.error()));
    CHECK(std::get<store::CodecError>(unknown_codec.error()).category
          == store::CodecErrorCategory::UnsupportedCodec);

    const std::array<uint8_t, 3> malformed_bytes{0xff, 0x00, 0x01};
    REQUIRE(io::write_bytes_to_path(malformed_bytes, index_path).has_value());
    const auto malformed = octree::open_folder_indexed(directory.path());
    REQUIRE_FALSE(malformed.has_value());
    REQUIRE(std::holds_alternative<store::IndexFormatError>(malformed.error()));
    CHECK(std::get<store::IndexFormatError>(malformed.error()).category
          == store::IndexFormatErrorCategory::Malformed);
}

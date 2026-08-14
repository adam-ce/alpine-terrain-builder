#include <atomic>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include "io/serialize.h"
#include "octree/StoreTraits.h"
#include "octree/store_layout/Mappings.h"
#include "raster_store/StoreTraits.h"
#include "store/IndexedStorage.h"
#include "store/codec/ZppBits.h"
#include "string_utils.h"

namespace {

class TemporaryDirectory {
public:
    explicit TemporaryDirectory(const std::string_view label) {
        static std::atomic_uint64_t counter = 0;
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        _path = std::filesystem::temp_directory_path()
            / ("atb-store-storage-" + std::string(label) + "-"
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

store::NodePath tile_to_path(const radix::tile::Id &key) {
    return store::NodePath(
        std::to_string(key.zoom_level) + "/" + std::to_string(key.coords.x)
        + "/" + std::to_string(key.coords.y));
}

std::optional<radix::tile::Id> path_to_tile(const store::NodePath &node_path) {
    const std::filesystem::path &path = node_path.path();
    if (path.is_absolute() || std::distance(path.begin(), path.end()) != 3) {
        return std::nullopt;
    }
    auto part = path.begin();
    const auto zoom = from_chars<unsigned>(part->string());
    ++part;
    const auto x = from_chars<uint32_t>(part->string());
    ++part;
    const auto y = from_chars<uint32_t>(part->string());
    if (!zoom.has_value() || !x.has_value() || !y.has_value()) {
        return std::nullopt;
    }
    const radix::tile::Id key{zoom.value(), {x.value(), y.value()}};
    return raster_store::StoreTraits::is_valid(key)
        ? std::optional<radix::tile::Id>(key)
        : std::nullopt;
}

template<typename Traits>
store::PathMapping<typename Traits::Key> test_mapping();

template<>
store::PathMapping<octree::Id> test_mapping<octree::StoreTraits>() {
    return octree::store_layout::flat();
}

template<>
store::PathMapping<radix::tile::Id> test_mapping<raster_store::StoreTraits>() {
    return {"test_tiles", tile_to_path, path_to_tile};
}

template<typename Traits>
store::Storage<Traits, int> make_storage(const std::filesystem::path &path) {
    return store::Storage<Traits, int>(store::RawStorage<Traits, int>(
        store::Layout<typename Traits::Key>(path, test_mapping<Traits>()),
        std::make_unique<store::codec::ZppBits<int>>()));
}

class MultiFileCodec final : public store::Codec<int> {
public:
    std::vector<std::filesystem::path> paths(const store::NodePath &node_path) const override {
        std::filesystem::path first = node_path.path();
        std::filesystem::path second = node_path.path();
        first += ".data";
        second += ".metadata";
        return {first, second};
    }
};

std::atomic_uint32_t index_writes = 0;

std::expected<store::IndexMetadata<octree::StoreTraits>, store::IndexFormatError>
unused_index_read(const std::filesystem::path &path) {
    return std::unexpected(store::IndexFormatError{
        store::IndexFormatErrorCategory::Open,
        path,
        "not used",
    });
}

std::expected<void, store::IndexFormatError> count_index_write(
    const std::filesystem::path &,
    const store::IndexMetadata<octree::StoreTraits> &) {
    ++index_writes;
    return {};
}

std::optional<store::PathMapping<octree::Id>> fake_mapping_from_id(
    const std::string_view id) {
    return id == "flat"
        ? std::optional<store::PathMapping<octree::Id>>(octree::store_layout::flat())
        : std::nullopt;
}

store::PathMapping<octree::Id> fake_default_mapping() {
    return octree::store_layout::flat();
}

store::IndexFormat<octree::StoreTraits> counting_index_format() {
    return {
        "terrain.index",
        unused_index_read,
        count_index_write,
        fake_mapping_from_id,
        fake_default_mapping,
    };
}

store::IndexedStorage<octree::StoreTraits, int> make_indexed_storage(
    const std::filesystem::path &path) {
    using Storage = store::Storage<octree::StoreTraits, int>;
    return store::IndexedStorage<octree::StoreTraits, int>(
        store::RawStorage<octree::StoreTraits, int>(
            store::Layout<octree::Id>(path, octree::store_layout::flat()),
            std::make_unique<store::codec::ZppBits<int>>()),
        store::Index<octree::StoreTraits>{},
        Storage::Persistence{
            counting_index_format(),
            path / "terrain.index",
            "flat",
            ".bin",
        });
}

} // namespace

TEMPLATE_TEST_CASE(
    "shared raw storage has no hidden octree dependency",
    "[store][storage]",
    octree::StoreTraits,
    raster_store::StoreTraits) {
    using Traits = TestType;
    TemporaryDirectory directory("traits");
    auto storage = make_storage<Traits>(directory.path());
    const auto root = Traits::root();

    CHECK_FALSE(storage.has(root).value());
    REQUIRE(storage.save(root, 42).has_value());
    CHECK(storage.has(root).value());
    REQUIRE(storage.load(root).has_value());
    CHECK(storage.load(root).value() == 42);
    REQUIRE(storage.remove(root).has_value());
    CHECK_FALSE(storage.has(root).value());
}

TEST_CASE("shared storage rejects invalid 2D keys", "[store][storage]") {
    TemporaryDirectory directory("invalid-key");
    auto storage = make_storage<raster_store::StoreTraits>(directory.path());
    const radix::tile::Id invalid{3, {8, 0}};

    CHECK(std::holds_alternative<store::InvalidKey<radix::tile::Id>>(
        storage.load(invalid).error()));
    CHECK(std::holds_alternative<store::InvalidKey<radix::tile::Id>>(
        storage.save(invalid, 1).error()));
    CHECK(std::holds_alternative<store::InvalidKey<radix::tile::Id>>(
        storage.has(invalid).error()));
    CHECK(std::holds_alternative<store::InvalidKey<radix::tile::Id>>(
        storage.remove(invalid).error()));
}

TEST_CASE("shared storage preserves overwrite settings", "[store][storage]") {
    TemporaryDirectory directory("overwrite");
    auto storage = make_storage<octree::StoreTraits>(directory.path());
    const octree::Id root = octree::Id::root();

    REQUIRE(storage.save(root, 1).has_value());
    const auto rejected = storage.save(root, 2);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(std::holds_alternative<store::AlreadyExists>(rejected.error()));
    CHECK(storage.load(root).value() == 1);

    storage.settings().allow_overwrite = true;
    REQUIRE(storage.save(root, 2).has_value());
    CHECK(storage.load(root).value() == 2);
}

TEST_CASE("raw storage requires and removes every codec path", "[store][storage]") {
    TemporaryDirectory directory("multi-file");
    store::RawStorage<octree::StoreTraits, int> storage(
        store::Layout<octree::Id>(directory.path(), octree::store_layout::flat()),
        std::make_unique<MultiFileCodec>());
    const octree::Id root = octree::Id::root();
    const auto paths = storage.paths(root).value();
    REQUIRE(paths.size() == 2);

    REQUIRE(io::write_to_path(1, paths[0]).has_value());
    CHECK_FALSE(storage.has(root).value());
    REQUIRE(io::write_to_path(2, paths[1]).has_value());
    CHECK(storage.has(root).value());
    REQUIRE(storage.remove(root).value());
    CHECK_FALSE(std::filesystem::exists(paths[0]));
    CHECK_FALSE(std::filesystem::exists(paths[1]));
}

TEST_CASE("storage moves transfer and finalize dirty index state", "[store][storage]") {
    index_writes = 0;
    TemporaryDirectory first_directory("move-first");
    TemporaryDirectory second_directory("move-second");

    {
        auto original = make_indexed_storage(first_directory.path());
        REQUIRE(original.save(octree::Id::root(), 1).has_value());
        auto moved = std::move(original);
        CHECK(index_writes == 0);
        REQUIRE(moved.save_index().has_value());
        CHECK(index_writes == 1);
    }
    CHECK(index_writes == 1);

    {
        auto destination = make_indexed_storage(first_directory.path());
        destination.settings().allow_overwrite = true;
        REQUIRE(destination.save(octree::Id::root(), 2).has_value());
        auto source = make_indexed_storage(second_directory.path());
        REQUIRE(source.save(octree::Id::root(), 3).has_value());
        destination = std::move(source);
        CHECK(index_writes == 2);
    }
    CHECK(index_writes == 3);
}

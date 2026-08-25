#include <atomic>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include "io/bytes.h"
#include "octree/StoreTraits.h"
#include "octree/store_layout/Mappings.h"
#include "raster_store/StoreTraits.h"
#include "store/IndexedStorage.h"
#include "string_utils.h"
#include "../temporary_directory.h"

namespace {

using test::TemporaryDirectory;

std::filesystem::path tile_to_path(const radix::tile::Id& key)
{
    return std::to_string(key.zoom_level) + "/" + std::to_string(key.coords.x) + "/" + std::to_string(key.coords.y);
}

std::optional<radix::tile::Id> path_to_tile(const std::filesystem::path& node_path)
{
    if (node_path.is_absolute() || std::distance(node_path.begin(), node_path.end()) != 3) {
        return std::nullopt;
    }
    auto part = node_path.begin();
    const auto zoom = from_chars<unsigned>(part->string());
    ++part;
    const auto x = from_chars<uint32_t>(part->string());
    ++part;
    const auto y = from_chars<uint32_t>(part->string());
    if (!zoom.has_value() || !x.has_value() || !y.has_value()) {
        return std::nullopt;
    }
    const radix::tile::Id key { zoom.value(), { x.value(), y.value() } };
    return raster_store::StoreTraits::is_valid(key) ? std::optional<radix::tile::Id>(key) : std::nullopt;
}

template <typename Traits>
store::PathMapping<typename Traits::Key> test_mapping();

template <>
store::PathMapping<octree::Id> test_mapping<octree::StoreTraits>()
{
    return octree::store_layout::flat();
}

template <>
store::PathMapping<radix::tile::Id> test_mapping<raster_store::StoreTraits>()
{
    return { "test_tiles", tile_to_path, path_to_tile };
}

std::expected<int, store::CodecError> read_int(const std::filesystem::path& path)
{
    auto bytes = io::read_bytes_from_path(path);
    if (!bytes || bytes->size() != sizeof(int)) {
        return std::unexpected(store::CodecError {
            store::CodecOperation::Read,
            store::CodecErrorCategory::Io,
            "test integer read failed",
        });
    }
    int value;
    std::memcpy(&value, bytes->data(), sizeof(value));
    return value;
}

std::expected<void, store::CodecError> write_int(const int value, const std::filesystem::path& path)
{
    const auto bytes = std::span(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
    if (!io::write_bytes_to_path(bytes, path)) {
        return std::unexpected(store::CodecError {
            store::CodecOperation::Write,
            store::CodecErrorCategory::Io,
            "test integer write failed",
        });
    }
    return {};
}

class IntCodec final : public store::Codec<int> {
public:
    std::vector<std::filesystem::path> paths(const std::filesystem::path& node_path) const override
    {
        return { add_extension(node_path, ".data") };
    }

    std::expected<int, store::CodecError> read(const std::filesystem::path& node_path) const override { return read_int(paths(node_path).front()); }

    std::expected<void, store::CodecError> write(const std::filesystem::path& node_path, const int& value) const override
    {
        return write_int(value, paths(node_path).front());
    }
};

template <typename Traits>
store::Storage<Traits, int> make_storage(const std::filesystem::path& path)
{
    return store::Storage<Traits, int>(
        store::RawStorage<Traits, int>(store::Layout<typename Traits::Key>(path, test_mapping<Traits>()), std::make_unique<IntCodec>()));
}

class MultiFileCodec final : public store::Codec<int> {
public:
    std::vector<std::filesystem::path> paths(const std::filesystem::path& node_path) const override
    {
        return { add_extension(node_path, ".data"), add_extension(node_path, ".metadata") };
    }

    std::expected<int, store::CodecError> read(const std::filesystem::path& node_path) const override
    {
        const auto value = read_int(paths(node_path).front());
        if (!value) {
            return std::unexpected(store::CodecError {
                store::CodecOperation::Read,
                store::CodecErrorCategory::Io,
                "multi-file read failed",
            });
        }
        return value.value();
    }

    std::expected<void, store::CodecError> write(const std::filesystem::path& node_path, const int& value) const override
    {
        const auto node_paths = paths(node_path);
        if (!write_int(value, node_paths[0]) || !write_int(value + 1, node_paths[1])) {
            return std::unexpected(store::CodecError {
                store::CodecOperation::Write,
                store::CodecErrorCategory::Io,
                "multi-file write failed",
            });
        }
        return {};
    }
};

class ConfigurableCodec final : public store::Codec<int> {
public:
    explicit ConfigurableCodec(std::string extension)
        : extension(std::move(extension))
    {
    }

    std::vector<std::filesystem::path> paths(const std::filesystem::path& node_path) const override
    {
        return { add_extension(node_path, extension) };
    }

    std::expected<int, store::CodecError> read(const std::filesystem::path& node_path) const override
    {
        if (fail_read) {
            return std::unexpected(store::CodecError {
                store::CodecOperation::Read,
                store::CodecErrorCategory::Domain,
                "injected decode failure",
            });
        }
        const auto value = read_int(paths(node_path).front());
        if (!value) {
            return std::unexpected(store::CodecError {
                store::CodecOperation::Read,
                store::CodecErrorCategory::Io,
                "configurable read failed",
            });
        }
        return value.value();
    }

    std::expected<void, store::CodecError> write(const std::filesystem::path& node_path, const int& value) const override
    {
        if (fail_write) {
            return std::unexpected(store::CodecError {
                store::CodecOperation::Write,
                store::CodecErrorCategory::Domain,
                "injected encode failure",
            });
        }
        if (!write_int(value, paths(node_path).front())) {
            return std::unexpected(store::CodecError {
                store::CodecOperation::Write,
                store::CodecErrorCategory::Io,
                "configurable write failed",
            });
        }
        return {};
    }

    std::string extension;
    bool fail_read = false;
    bool fail_write = false;
};

std::atomic_uint32_t index_writes = 0;

std::expected<store::IndexMetadata<octree::StoreTraits>, store::IndexFormatError> unused_index_read(const std::filesystem::path& path)
{
    return std::unexpected(store::IndexFormatError {
        store::IndexFormatErrorCategory::Open,
        path,
        "not used",
    });
}

std::expected<void, store::IndexFormatError> count_index_write(const std::filesystem::path&, const store::IndexMetadata<octree::StoreTraits>&)
{
    ++index_writes;
    return {};
}

std::optional<store::PathMapping<octree::Id>> fake_mapping_from_id(const std::string_view id)
{
    return id == "flat" ? std::optional<store::PathMapping<octree::Id>>(octree::store_layout::flat()) : std::nullopt;
}

store::PathMapping<octree::Id> fake_default_mapping() { return octree::store_layout::flat(); }

store::IndexFormat<octree::StoreTraits> counting_index_format()
{
    return {
        "octree.storeindex",
        unused_index_read,
        count_index_write,
        fake_mapping_from_id,
        fake_default_mapping,
    };
}

store::IndexedStorage<octree::StoreTraits, int> make_indexed_storage(
    const std::filesystem::path& path, std::unique_ptr<store::Codec<int>> codec = std::make_unique<IntCodec>())
{
    using Storage = store::Storage<octree::StoreTraits, int>;
    return store::IndexedStorage<octree::StoreTraits, int>(
        store::RawStorage<octree::StoreTraits, int>(store::Layout<octree::Id>(path, octree::store_layout::flat()), std::move(codec)),
        store::Index<octree::StoreTraits> {},
        Storage::Persistence {
            counting_index_format(),
            path / "octree.storeindex",
            "flat",
            "test.Int",
            ".data",
        });
}

} // namespace

TEST_CASE("byte writes report directory creation errors", "[io][bytes]")
{
    TemporaryDirectory directory("write-parent-error");
    const std::filesystem::path blocker = directory.path() / "blocker";
    REQUIRE(write_int(1, blocker).has_value());

    const auto result = io::write_bytes_to_path(std::span<const uint8_t> {}, blocker / "payload");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == io::Error(io::Error::CreateDirectories));
}

TEMPLATE_TEST_CASE("shared raw storage has no hidden octree dependency", "[store][storage]", octree::StoreTraits, raster_store::StoreTraits)
{
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

TEST_CASE("shared storage rejects invalid 2D keys", "[store][storage]")
{
    TemporaryDirectory directory("invalid-key");
    auto storage = make_storage<raster_store::StoreTraits>(directory.path());
    const radix::tile::Id invalid { 3, { 8, 0 } };

    CHECK(std::holds_alternative<store::InvalidKey<radix::tile::Id>>(storage.load(invalid).error()));
    CHECK(std::holds_alternative<store::InvalidKey<radix::tile::Id>>(storage.save(invalid, 1).error()));
    CHECK(std::holds_alternative<store::InvalidKey<radix::tile::Id>>(storage.has(invalid).error()));
    CHECK(std::holds_alternative<store::InvalidKey<radix::tile::Id>>(storage.remove(invalid).error()));
}

TEST_CASE("shared storage preserves overwrite settings", "[store][storage]")
{
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

TEST_CASE("raw storage requires and removes every codec path", "[store][storage]")
{
    TemporaryDirectory directory("multi-file");
    store::RawStorage<octree::StoreTraits, int> storage(
        store::Layout<octree::Id>(directory.path(), octree::store_layout::flat()), std::make_unique<MultiFileCodec>());
    const octree::Id root = octree::Id::root();
    const auto paths = storage.paths(root).value();
    REQUIRE(paths.size() == 2);

    REQUIRE(write_int(1, paths[0]).has_value());
    CHECK_FALSE(storage.has(root).value());
    REQUIRE(write_int(2, paths[1]).has_value());
    CHECK(storage.has(root).value());
    REQUIRE(storage.remove(root).value());
    CHECK_FALSE(std::filesystem::exists(paths[0]));
    CHECK_FALSE(std::filesystem::exists(paths[1]));
}

TEST_CASE("storage moves transfer and finalize dirty index state", "[store][storage]")
{
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

TEST_CASE("copy_from hard-links every matching codec file", "[store][storage][copy]")
{
    const octree::Id root = octree::Id::root();

    SECTION("one file")
    {
        TemporaryDirectory source_directory("copy-one-source");
        TemporaryDirectory target_directory("copy-one-target");
        auto source = make_indexed_storage(source_directory.path());
        auto target = make_indexed_storage(target_directory.path());
        REQUIRE(source.save(root, 41).has_value());

        REQUIRE(target.copy_from(root, source).has_value());
        REQUIRE(target.has(root).value());
        CHECK(target.load(root).value() == 41);
        CHECK(std::filesystem::equivalent(source.paths(root)->front(), target.paths(root)->front()));
    }

    SECTION("multiple files")
    {
        TemporaryDirectory source_directory("copy-many-source");
        TemporaryDirectory target_directory("copy-many-target");
        auto source = make_indexed_storage(source_directory.path(), std::make_unique<MultiFileCodec>());
        auto target = make_indexed_storage(target_directory.path(), std::make_unique<MultiFileCodec>());
        REQUIRE(source.save(root, 42).has_value());

        REQUIRE(target.copy_from(root, source).has_value());
        const auto source_paths = source.paths(root).value();
        const auto target_paths = target.paths(root).value();
        REQUIRE(source_paths.size() == 2);
        REQUIRE(target_paths.size() == 2);
        CHECK(std::filesystem::equivalent(source_paths[0], target_paths[0]));
        CHECK(std::filesystem::equivalent(source_paths[1], target_paths[1]));
    }

    SECTION("same storage")
    {
        TemporaryDirectory directory("copy-self");
        auto storage = make_indexed_storage(directory.path());
        REQUIRE(storage.save(root, 43).has_value());
        storage.settings().allow_overwrite = true;

        REQUIRE(storage.copy_from(root, storage).has_value());
        CHECK(storage.load(root).value() == 43);
    }
}

TEST_CASE("copy_from re-encodes differing path lists", "[store][storage][copy]")
{
    const octree::Id root = octree::Id::root();
    TemporaryDirectory source_directory("copy-reencode-source");
    TemporaryDirectory target_directory("copy-reencode-target");
    auto source = make_indexed_storage(source_directory.path(), std::make_unique<MultiFileCodec>());
    auto target = make_indexed_storage(target_directory.path());
    REQUIRE(source.save(root, 43).has_value());

    REQUIRE(target.copy_from(root, source).has_value());
    CHECK(target.load(root).value() == 43);
    CHECK(target.paths(root)->size() == 1);
    CHECK(source.paths(root)->size() == 2);
    CHECK_FALSE(std::filesystem::equivalent(source.paths(root)->front(), target.paths(root)->front()));
}

TEST_CASE("copy_from enforces overwrite settings", "[store][storage][copy]")
{
    const octree::Id root = octree::Id::root();
    TemporaryDirectory source_directory("copy-overwrite-source");
    TemporaryDirectory target_directory("copy-overwrite-target");
    auto source = make_indexed_storage(source_directory.path());
    auto target = make_indexed_storage(target_directory.path());
    REQUIRE(source.save(root, 44).has_value());
    REQUIRE(target.save(root, 1).has_value());

    const auto rejected = target.copy_from(root, source);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(std::holds_alternative<store::AlreadyExists>(rejected.error()));
    CHECK(target.load(root).value() == 1);

    target.settings().allow_overwrite = true;
    REQUIRE(target.copy_from(root, source).has_value());
    CHECK(target.load(root).value() == 44);
    CHECK(std::filesystem::equivalent(source.paths(root)->front(), target.paths(root)->front()));
}

TEST_CASE("failed multi-file overwrite leaves the target node unindexed", "[store][storage][copy]")
{
    const octree::Id root = octree::Id::root();
    TemporaryDirectory source_directory("copy-partial-source");
    TemporaryDirectory target_directory("copy-partial-target");
    auto source = make_indexed_storage(source_directory.path(), std::make_unique<MultiFileCodec>());
    auto target = make_indexed_storage(target_directory.path(), std::make_unique<MultiFileCodec>());
    REQUIRE(source.save(root, 45).has_value());
    REQUIRE(target.save(root, 1).has_value());
    target.settings().allow_overwrite = true;

    const auto target_paths = target.paths(root).value();
    REQUIRE(std::filesystem::remove(target_paths[1]));
    REQUIRE(std::filesystem::create_directory(target_paths[1]));
    REQUIRE(write_int(9, target_paths[1] / "blocker.data").has_value());

    const auto copied = target.copy_from(root, source);
    REQUIRE_FALSE(copied.has_value());
    CHECK(std::holds_alternative<store::FilesystemError>(copied.error()));
    CHECK_FALSE(target.has(root).value());
    CHECK_FALSE(target.index().get(root).value().has_value());
    CHECK(std::filesystem::equivalent(source.paths(root)->front(), target_paths.front()));
    CHECK(std::filesystem::is_directory(target_paths[1]));
}

TEST_CASE("copy_from propagates source and codec failures", "[store][storage][copy]")
{
    const octree::Id root = octree::Id::root();

    SECTION("missing source")
    {
        TemporaryDirectory source_directory("copy-missing-source");
        TemporaryDirectory target_directory("copy-missing-target");
        auto source = make_indexed_storage(source_directory.path());
        auto target = make_indexed_storage(target_directory.path());
        REQUIRE(target.save(root, 7).has_value());
        target.settings().allow_overwrite = true;

        const auto copied = target.copy_from(root, source);
        REQUIRE_FALSE(copied.has_value());
        CHECK(std::holds_alternative<store::MissingSource<octree::Id>>(copied.error()));
        CHECK(target.has(root).value());
        CHECK(target.load(root).value() == 7);
    }

    SECTION("decode failure")
    {
        TemporaryDirectory source_directory("copy-decode-source");
        TemporaryDirectory target_directory("copy-decode-target");
        auto source_codec = std::make_unique<ConfigurableCodec>(".source");
        ConfigurableCodec* source_codec_ptr = source_codec.get();
        auto source = make_indexed_storage(source_directory.path(), std::move(source_codec));
        auto target = make_indexed_storage(target_directory.path(), std::make_unique<ConfigurableCodec>(".target"));
        REQUIRE(source.save(root, 46).has_value());
        REQUIRE(target.save(root, 7).has_value());
        target.settings().allow_overwrite = true;
        source_codec_ptr->fail_read = true;

        const auto copied = target.copy_from(root, source);
        REQUIRE_FALSE(copied.has_value());
        REQUIRE(std::holds_alternative<store::CodecError>(copied.error()));
        CHECK(std::get<store::CodecError>(copied.error()).operation == store::CodecOperation::Read);
        CHECK(target.has(root).value());
        CHECK(target.load(root).value() == 7);
    }

    SECTION("encode failure")
    {
        TemporaryDirectory source_directory("copy-encode-source");
        TemporaryDirectory target_directory("copy-encode-target");
        auto source = make_indexed_storage(source_directory.path(), std::make_unique<ConfigurableCodec>(".source"));
        auto target_codec = std::make_unique<ConfigurableCodec>(".target");
        ConfigurableCodec* target_codec_ptr = target_codec.get();
        auto target = make_indexed_storage(target_directory.path(), std::move(target_codec));
        REQUIRE(source.save(root, 47).has_value());
        REQUIRE(target.save(root, 7).has_value());
        target.settings().allow_overwrite = true;
        target_codec_ptr->fail_write = true;

        const auto copied = target.copy_from(root, source);
        REQUIRE_FALSE(copied.has_value());
        REQUIRE(std::holds_alternative<store::CodecError>(copied.error()));
        CHECK(std::get<store::CodecError>(copied.error()).operation == store::CodecOperation::Write);
        CHECK_FALSE(target.has(root).value());
    }
}

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/type_precision.hpp>

#include "../temporary_directory.h"
#include "io/bytes.h"
#include "raster_store/storage.h"

namespace {

using test::TemporaryDirectory;
namespace attribution = raster_store::attribution;
namespace storage = raster_store::storage;
namespace manifest = raster_store::io::manifest;
namespace tile_codec = raster_store::io::tile_codec;
using raster_store::io::TileCodec;

constexpr std::string_view entity_json =
    R"({"spatial_resolution":0.25,"acquisition_date":"summer 2020","ingestion_date":"2026-09-08",)"
    R"("copyright":"Example","copyright_link":"https://example.org","license":"Example license"})";

void write_text(const std::filesystem::path& path, const std::string_view text)
{
    REQUIRE(io::write_bytes_to_path(std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()), path));
}

void write_table(const std::filesystem::path& directory)
{
    write_text(directory / attribution::file_name, "[" + std::string(entity_json) + "]");
}

storage::CreateOptions small_options()
{
    storage::CreateOptions options;
    options.tile_dimensions = { 3, 3 };
    return options;
}

template <typename T>
void check_bytes(const radix::Raster<T>& left, const radix::Raster<T>& right)
{
    CHECK(left.size() == right.size());
    CHECK(std::ranges::equal(left.bytes(), right.bytes()));
}

} // namespace

TEST_CASE("attribution tables preserve complete ordinary objects", "[raster-store][attribution]")
{
    auto table = attribution::parse("[" + std::string(entity_json) + "]");
    REQUIRE(table);
    REQUIRE(table->entities.size() == 1);
    REQUIRE(table->at(0));
    CHECK((*table->at(0))->spatial_resolution == 0.25);
    CHECK((*table->at(0))->acquisition_date == "summer 2020");
    CHECK(table->at(1).error().code() == Error::Code::InvalidInput);
    CHECK(table->at(65535).error().code() == Error::Code::Unsupported);
    CHECK(table->at(UINT32_MAX).error().code() == Error::Code::Unsupported);
    table->entities.resize(65535);
    CHECK(table->at(65534));

    const auto ordinary_empty_values = attribution::parse(
        R"([{"spatial_resolution":0,"acquisition_date":"","ingestion_date":"","copyright":"","copyright_link":"","license":""}])");
    REQUIRE(ordinary_empty_values);
    CHECK(ordinary_empty_values->entities[0].copyright.empty());
}

TEST_CASE("attribution tables reject malformed slots and excessive indices", "[raster-store][attribution]")
{
    for (const auto json : { "{}", "[]", "[null]", "[{}]", "[1]", "not json" }) {
        INFO(json);
        auto table = attribution::parse(json);
        REQUIRE_FALSE(table);
        CHECK(table.error().code() == Error::Code::CorruptData);
    }
    auto wrong_resolution = std::string(entity_json);
    wrong_resolution.replace(wrong_resolution.find("0.25"), 4, "\"0.25\"");
    CHECK_FALSE(attribution::parse("[" + wrong_resolution + "]"));
    auto wrong_date = std::string(entity_json);
    wrong_date.replace(wrong_date.find("\"summer 2020\""), 13, "null");
    CHECK_FALSE(attribution::parse("[" + wrong_date + "]"));

    std::string excessive = "[null";
    for (unsigned index = 1; index < 65536; ++index) {
        excessive += ",null";
    }
    excessive += "]";
    CHECK(attribution::parse(excessive).error().code() == Error::Code::Unsupported);
}

TEST_CASE("attribution lookup stops at the nearest of exactly three locations", "[raster-store][attribution]")
{
    TemporaryDirectory directory;
    const auto grandparent = directory.path() / "collection";
    const auto parent = grandparent / "layer";
    const auto snapshot = parent / "snapshot";
    const auto index = snapshot / manifest::index_file_name;
    write_table(directory.path());
    CHECK(attribution::find_table(index).error().code() == Error::Code::NotFound);
    write_table(grandparent);
    CHECK(attribution::find_table(index).value() == grandparent / attribution::file_name);
    write_table(parent);
    CHECK(attribution::find_table(index).value() == parent / attribution::file_name);
    write_table(snapshot);
    CHECK(attribution::find_table(index).value() == snapshot / attribution::file_name);
    write_text(snapshot / attribution::file_name, "[null]");
    CHECK(attribution::read_table(index).error().code() == Error::Code::CorruptData);
    std::filesystem::remove(snapshot / attribution::file_name);
    std::filesystem::create_symlink(snapshot / "missing", snapshot / attribution::file_name);
    CHECK(attribution::find_table(index).value() == snapshot / attribution::file_name);
    CHECK_FALSE(attribution::read_table(index));
}

TEMPLATE_TEST_CASE("AMORT preserves native scalar and packed vector bytes", "[raster-store][codec]",
    float, double, std::int16_t, std::uint32_t, glm::vec3, glm::u8vec3)
{
    TemporaryDirectory directory;
    raster_store::Tile<TestType> tile(3);
    CHECK(std::ranges::all_of(tile.source_attribution, [](const auto value) { return value == 0; }));
    std::uint8_t value = 0;
    for (auto& byte : tile.data.bytes()) {
        byte = std::byte(value);
        value = static_cast<std::uint8_t>(value + 37);
    }
    tile.source_attribution.pixel({ 1, 1 }) = 65534;
    const auto path = directory.path() / "tile";
    TileCodec<TestType> writer({ 3, 3 });
    REQUIRE(writer.write(path, tile));
    auto read = writer.read(path);
    REQUIRE(read);
    check_bytes(read->data, tile.data);
    check_bytes(read->source_attribution, tile.source_attribution);

    TileCodec<TestType> uncompressed({ 3, 3 }, io::envelope::CompressionAlgorithm::None, io::envelope::ChecksumAlgorithm::Crc32c);
    REQUIRE(uncompressed.write(path, tile));
    read = writer.read(path);
    REQUIRE(read);
    check_bytes(read->data, tile.data);
}

TEST_CASE("AMORT preserves NaN bits and data beneath NoData pixels", "[raster-store][codec]")
{
    TemporaryDirectory directory;
    raster_store::Tile<float> tile(3);
    tile.data.pixel({ 0, 0 }) = std::bit_cast<float>(std::uint32_t { 0x7fc01234 });
    tile.source_attribution.pixel({ 0, 0 }) = 1;
    tile.data.pixel({ 1, 0 }) = 42.0f;
    tile.data.pixel({ 2, 0 }) = std::bit_cast<float>(std::uint32_t { 0x80000000 });
    TileCodec<float> amort({ 3, 3 });
    REQUIRE(amort.write(directory.path() / "tile", tile));
    auto read = amort.read(directory.path() / "tile");
    REQUIRE(read);
    check_bytes(read->data, tile.data);
    check_bytes(read->source_attribution, tile.source_attribution);
}

TEST_CASE("AMORT validates dimensions and byte counts before allocating typed rasters", "[raster-store][codec]")
{
    TemporaryDirectory directory;
    const auto path = directory.path() / "tile";
    TileCodec<float> amort({ 3, 3 });
    raster_store::Tile<float> tile(3);
    tile.source_attribution = radix::Raster<std::uint16_t>(2);
    CHECK(amort.write(path, tile).error().code() == Error::Code::InvalidInput);
    TileCodec<float> rectangular({ 3, 2 });
    CHECK(rectangular.write(path, tile).error().code() == Error::Code::InvalidInput);

    tile_codec::detail::v1::RasterTile encoded { 3, 3, io::envelope::Bytes(36), io::envelope::Bytes(18) };
    SECTION("wrong dimensions") { encoded.width = 4; }
    SECTION("short data") { encoded.data.pop_back(); }
    SECTION("long attribution") { encoded.source_attribution.push_back(std::byte { 0 }); }
    REQUIRE(io::envelope::write_to_path<tile_codec::TileSchema>(encoded, path.string() + ".amort"));
    CHECK(amort.read(path).error().code() == Error::Code::CorruptData);
}

TEST_CASE("raster XYZ paths round trip boundary IDs and reject malformed paths", "[raster-store][layout]")
{
    const auto mapping = raster_store::path_layout::zoom_xy_google::zoom_x_y_google();
    CHECK(mapping.id == "zoom/x/y_google");
    for (const auto key : { radix::tile::Id { 0, { 0, 0 } }, radix::tile::Id { 32, { UINT32_MAX, UINT32_MAX } } }) {
        CHECK(mapping.node_path_to_key(mapping.key_to_node_path(key)) == key);
    }
    for (const auto path : { "", "/0/0/0", "0/1/0", "33/0/0", "1/0/0.amort", "1/0/0/extra", "1/0/0junk", "1/../0" }) {
        INFO(path);
        CHECK_FALSE(mapping.node_path_to_key(path));
    }
}

TEST_CASE("raster index adapters preserve mixed hierarchy and reject corrupt topology", "[raster-store][index]")
{
    store::Index<raster_store::StoreTraits> index;
    const auto root = raster_store::StoreTraits::root();
    REQUIRE(index.add(root));
    REQUIRE(index.add({ 2, { 1, 1 } }));
    auto encoded = manifest::encode_index(index);
    auto decoded = manifest::decode_index(encoded);
    REQUIRE(decoded);
    CHECK(decoded->is(store::NodeStatus::Inner, root).value());
    CHECK(decoded->is(store::NodeStatus::Virtual, { 1, { 0, 0 } }).value());
    CHECK(decoded->is(store::NodeStatus::Leaf, { 2, { 1, 1 } }).value());
    REQUIRE(encoded.entries.size() == 3);
    CHECK(encoded.entries[0].id == root);
    CHECK(encoded.entries[1].id == radix::tile::Id { 1, { 0, 0 } });
    CHECK(encoded.entries[2].id == radix::tile::Id { 2, { 1, 1 } });

    SECTION("duplicate key") { encoded.entries.push_back(encoded.entries[0]); }
    SECTION("invalid key") { encoded.entries[0].id.coords.x = 1; }
    SECTION("invalid status") { encoded.entries[0].status = static_cast<store::NodeStatus::Value>(255); }
    SECTION("missing parent") { encoded.entries.erase(encoded.entries.begin() + 1); }
    SECTION("leaf with descendant") { encoded.entries[0].status = store::NodeStatus::Leaf; }
    SECTION("virtual without children") { encoded.entries.pop_back(); }
    CHECK(manifest::decode_index(encoded).error().code() == Error::Code::CorruptData);
}

TEST_CASE("raster snapshots checkpoint metadata dimensions and publish explicitly", "[raster-store][storage]")
{
    TemporaryDirectory directory;
    write_table(directory.path());
    const auto final = directory.path() / "snapshot";
    const auto partial = directory.path() / "snapshot.part";
    auto created = storage::create<float>(final, small_options());
    REQUIRE(created);
    CHECK_FALSE(storage::open<float>(partial));
    CHECK_FALSE(storage::open<float>(partial / ""));
    CHECK(storage::open<float>(partial, { .allow_incomplete = true }));
    const auto metadata_before = io::read_bytes_from_path(partial / manifest::metadata_file_name).value();
    raster_store::Tile<float> tile(3);
    tile.data.fill(12.5f);
    // Reference checks belong to RF builder, not storage reads or writes.
    tile.source_attribution.fill(65534);
    REQUIRE(created->save({ 0, { 0, 0 } }, tile));
    REQUIRE(created->save_index());
    CHECK(io::read_bytes_from_path(partial / manifest::metadata_file_name).value() == metadata_before);
    auto checkpoint = storage::open<float>(partial, { .allow_incomplete = true });
    REQUIRE(checkpoint);
    REQUIRE(checkpoint->load({ 0, { 0, 0 } }));
    CHECK(checkpoint->codec_selector().value() == "amort");
    auto metadata = manifest::read_metadata(partial);
    REQUIRE(metadata);
    CHECK(metadata->width == 3);
    CHECK(metadata->height == 3);
    CHECK(metadata->payload_type == "float32");
    CHECK(metadata->codec_selector == "amort");

    REQUIRE(storage::publish(std::move(*created)));
    CHECK_FALSE(std::filesystem::exists(partial));
    auto published = storage::open<float>(final);
    REQUIRE(published);
    CHECK(published->base_path() == final);
    check_bytes(published->load({ 0, { 0, 0 } })->data, tile.data);
    CHECK(storage::open<std::uint32_t>(final).error().code() == Error::Code::Unsupported);
}

TEST_CASE("failed checkpoints retain the previous index and ignore unindexed files", "[raster-store][storage]")
{
    TemporaryDirectory directory;
    write_table(directory.path());
    auto created = storage::create<float>(directory.path() / "snapshot", small_options());
    REQUIRE(created);
    const auto partial = created->base_path();
    REQUIRE(created->save({ 0, { 0, 0 } }, raster_store::Tile<float>(3)));
    std::filesystem::create_directory(partial / "raster_store.index.tmp");
    CHECK_FALSE(created->save_index());
    auto checkpoint = storage::open<float>(partial, { .allow_incomplete = true });
    REQUIRE(checkpoint);
    CHECK(checkpoint->index().empty());
    CHECK(checkpoint->load({ 0, { 0, 0 } }).error().code() == Error::Code::NotFound);
    std::filesystem::remove(partial / "raster_store.index.tmp");
    REQUIRE(created->save_index());
    CHECK(storage::open<float>(partial, { .allow_incomplete = true })->index().size() == 1);
}

TEST_CASE("raster creation and publication reject destination collisions", "[raster-store][storage]")
{
    TemporaryDirectory directory;
    write_table(directory.path());
    const auto final = directory.path() / "snapshot";
    auto created = storage::create<float>(final, small_options());
    REQUIRE(created);
    CHECK(storage::create<float>(final, small_options()).error().code() == Error::Code::AlreadyExists);
    SECTION("empty destination") { std::filesystem::create_directory(final); }
    SECTION("nonempty destination") { write_text(final / "keep", "keep"); }
    SECTION("dangling symlink") { std::filesystem::create_symlink(directory.path() / "missing", final); }
    CHECK(storage::publish(std::move(*created)).error().code() == Error::Code::AlreadyExists);
    CHECK(std::filesystem::exists(directory.path() / "snapshot.part"));
    CHECK(storage::create<float>(final, small_options()).error().code() == Error::Code::AlreadyExists);
}

TEST_CASE("raster publication does not scan payloads or reopen output", "[raster-store][storage]")
{
    TemporaryDirectory directory;
    write_table(directory.path());
    const auto final = directory.path() / "snapshot";
    auto created = storage::create<float>(final, small_options());
    REQUIRE(created);
    REQUIRE(created->save({ 0, { 0, 0 } }, raster_store::Tile<float>(3)));
    REQUIRE(std::filesystem::remove(created->path_for({ 0, { 0, 0 } }).value()));
    REQUIRE(storage::publish(std::move(*created)));
    auto opened = storage::open<float>(final);
    REQUIRE(opened);
    CHECK(opened->load({ 0, { 0, 0 } }).error().code() == Error::Code::NotFound);
}

TEST_CASE("raster reader selection follows metadata instead of file endings", "[raster-store][storage]")
{
    TemporaryDirectory directory;
    write_table(directory.path());
    const auto final = directory.path() / "snapshot";
    auto created = storage::create<float>(final, small_options());
    REQUIRE(created);
    REQUIRE(created->save({ 0, { 0, 0 } }, raster_store::Tile<float>(3)));
    REQUIRE(storage::publish(std::move(*created)));
    auto metadata = manifest::read_metadata(final).value();
    metadata.codec_selector = ".amort";
    REQUIRE(io::envelope::write_to_path<manifest::MetadataSchema>(metadata, final / manifest::metadata_file_name));
    CHECK(storage::open<float>(final).error().code() == Error::Code::Unsupported);
    metadata.codec_selector = "amort";
    REQUIRE(io::envelope::write_to_path<manifest::MetadataSchema>(metadata, final / manifest::metadata_file_name));
    REQUIRE(storage::open<float>(final));
}

TEST_CASE("cross-root hard links and independent attribution survive RF removal", "[raster-store][storage][copy]")
{
    TemporaryDirectory directory;
    const auto rf_root = directory.path() / "rf";
    const auto tb_root = directory.path() / "tb";
    const auto rf = rf_root / "snapshot";
    const auto tb = tb_root / "snapshot";
    write_table(rf_root);
    auto created = storage::create<float>(rf, small_options());
    REQUIRE(created);
    raster_store::Tile<float> tile(3);
    tile.data.fill(12.5f);
    REQUIRE(created->save({ 0, { 0, 0 } }, tile));
    REQUIRE(created->save({ 2, { 1, 1 } }, tile));
    REQUIRE(storage::publish(std::move(*created)));
    {
        auto source = storage::open<float>(rf);
        REQUIRE(source);
        auto options = small_options();
        options.copy_attribution_from_index = rf / manifest::index_file_name;
        auto target = storage::create<float>(tb, options);
        REQUIRE(target);
        for (const auto key : { radix::tile::Id { 0, { 0, 0 } }, radix::tile::Id { 2, { 1, 1 } } }) {
            REQUIRE(target->copy_from(key, *source));
            CHECK(std::filesystem::equivalent(source->path_for(key).value(), target->path_for(key).value()));
        }
        CHECK_FALSE(std::filesystem::equivalent(rf_root / attribution::file_name, target->base_path() / attribution::file_name));
        CHECK(io::read_bytes_from_path(rf_root / attribution::file_name).value()
            == io::read_bytes_from_path(target->base_path() / attribution::file_name).value());
        REQUIRE(storage::publish(std::move(*target)));
    }
    std::filesystem::remove_all(rf_root);
    auto target = storage::open<float>(tb);
    REQUIRE(target);
    CHECK(target->index().is(store::NodeStatus::Inner, { 0, { 0, 0 } }).value());
    CHECK(target->load({ 2, { 1, 1 } })->data.pixel({ 0, 0 }) == 12.5f);
    CHECK(attribution::read_table(tb / manifest::index_file_name));
}

TEST_CASE("byte writes report buffered device failures", "[io][bytes][raster-store]")
{
#ifdef __linux__
    const std::array<std::uint8_t, 1> bytes { 42 };
    auto written = io::write_bytes_to_path(bytes, "/dev/full", false);
    REQUIRE_FALSE(written);
    CHECK(written.error().code() == Error::Code::Io);
#endif
}

TEST_CASE("snapshot rename itself refuses an existing empty destination", "[raster-store][storage]")
{
    TemporaryDirectory directory;
    const auto source = directory.path() / "source";
    const auto destination = directory.path() / "destination";
    write_text(source / "keep", "payload");
    std::filesystem::create_directory(destination);
    auto renamed = io::utils::rename_without_replacement(source, destination);
    REQUIRE_FALSE(renamed);
    CHECK(renamed.error().code() == Error::Code::AlreadyExists);
    CHECK(std::filesystem::exists(source / "keep"));
    CHECK(std::filesystem::is_empty(destination));
}

TEST_CASE("snapshot rename reports missing paths", "[raster-store][storage]")
{
    TemporaryDirectory directory;
    const auto source = directory.path() / "source";
    auto destination = directory.path() / "destination";
    SECTION("missing source") { }
    SECTION("missing destination parent")
    {
        write_text(source / "keep", "payload");
        destination = directory.path() / "missing" / "destination";
    }
    auto renamed = io::utils::rename_without_replacement(source, destination);
    REQUIRE_FALSE(renamed);
    CHECK(renamed.error().code() == Error::Code::NotFound);
}

TEST_CASE("attribution copy reports missing paths", "[raster-store][attribution]")
{
    TemporaryDirectory directory;
    const auto source = directory.path() / "source";
    const auto destination = directory.path() / "destination";
    write_table(source);
    SECTION("dangling source symlink")
    {
        REQUIRE(std::filesystem::remove(source / attribution::file_name));
        std::filesystem::create_symlink(source / "missing", source / attribution::file_name);
        std::filesystem::create_directory(destination);
    }
    SECTION("missing destination directory") { }
    auto copied = attribution::copy_table(source / manifest::index_file_name, destination);
    REQUIRE_FALSE(copied);
    CHECK(copied.error().code() == Error::Code::NotFound);
}

TEST_CASE("raster opening retains metadata and index errors without creating a dataset", "[raster-store][storage]")
{
    TemporaryDirectory directory;
    write_table(directory.path());
    const auto final = directory.path() / "snapshot";
    CHECK(storage::open<float>(final).error().code() == Error::Code::NotFound);
    CHECK_FALSE(std::filesystem::exists(final));
    auto created = storage::create<float>(final, small_options());
    REQUIRE(created);
    REQUIRE(storage::publish(std::move(*created)));
    SECTION("missing metadata")
    {
        std::filesystem::remove(final / manifest::metadata_file_name);
        CHECK(storage::open<float>(final).error().code() == Error::Code::NotFound);
    }
    SECTION("missing index")
    {
        std::filesystem::remove(final / manifest::index_file_name);
        CHECK(storage::open<float>(final).error().code() == Error::Code::NotFound);
    }
    SECTION("unknown layout")
    {
        auto metadata = manifest::read_metadata(final).value();
        metadata.layout_id = "unknown";
        REQUIRE(io::envelope::write_to_path<manifest::MetadataSchema>(metadata, final / manifest::metadata_file_name));
        CHECK(storage::open<float>(final).error().code() == Error::Code::Unsupported);
    }
    SECTION("rectangular metadata")
    {
        auto metadata = manifest::read_metadata(final).value();
        metadata.height = 2;
        REQUIRE(io::envelope::write_to_path<manifest::MetadataSchema>(metadata, final / manifest::metadata_file_name));
        CHECK(storage::open<float>(final).error().code() == Error::Code::CorruptData);
    }
    SECTION("wrong envelope class")
    {
        std::filesystem::copy_file(final / manifest::metadata_file_name, final / manifest::index_file_name,
            std::filesystem::copy_options::overwrite_existing);
        CHECK(storage::open<float>(final).error().code() == Error::Code::CorruptData);
    }
}

TEST_CASE("AMORT propagates envelope checksum and version errors", "[raster-store][codec]")
{
    TemporaryDirectory directory;
    const auto path = directory.path() / "tile";
    const auto payload_path = directory.path() / "tile.amort";
    TileCodec<float> amort({ 3, 3 });
    REQUIRE(amort.write(path, raster_store::Tile<float>(3)));
    const auto bytes = io::read_bytes_from_path(payload_path).value();
    auto envelope = io::envelope::detail::deserialize_from_bytes<io::envelope::Envelope>(std::as_bytes(std::span(bytes))).value();
    auto expected_error = Error::Code::CorruptData;
    SECTION("checksum") { envelope.compressed_data.back() ^= std::byte { 1 }; }
    SECTION("version")
    {
        envelope.class_version = 999;
        expected_error = Error::Code::Unsupported;
    }
    const auto modified = io::envelope::detail::serialize_to_bytes(envelope).value();
    REQUIRE(io::write_bytes_to_path(std::span(reinterpret_cast<const std::uint8_t*>(modified.data()), modified.size()), payload_path));
    CHECK(amort.read(path).error().code() == expected_error);
}

TEST_CASE("abandoned raster output checkpoints but never publishes", "[raster-store][storage]")
{
    TemporaryDirectory directory;
    write_table(directory.path());
    const auto final = directory.path() / "snapshot";
    const auto partial = directory.path() / "snapshot.part";
    {
        auto created = storage::create<float>(final, small_options());
        REQUIRE(created);
        REQUIRE(created->save({ 0, { 0, 0 } }, raster_store::Tile<float>(3)));
    }
    CHECK_FALSE(std::filesystem::exists(final));
    auto abandoned = storage::open<float>(partial, { .allow_incomplete = true });
    REQUIRE(abandoned);
    CHECK(abandoned->load({ 0, { 0, 0 } }));
}

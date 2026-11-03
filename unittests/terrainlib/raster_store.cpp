/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>

#include "../temporary_directory.h"
#include "io/byte.h"
#include "io/image.h"
#include "io/utils.h"
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
    REQUIRE(io::byte::write(std::as_bytes(std::span(text)), path, io::byte::WriteMode::Overwrite));
}

void write_table(const std::filesystem::path& directory)
{
    write_text(directory / attribution::file_name, "[" + std::string(entity_json) + "]");
}

storage::CreateOptions small_options()
{
    storage::CreateOptions options;
    options.nominal_tile_size = 4;
    return options;
}

template <typename T>
void check_bytes(const radix::Raster<T>& left, const radix::Raster<T>& right)
{
    CHECK(left.size() == right.size());
    CHECK(std::ranges::equal(left.bytes(), right.bytes()));
}

void write_container(const tile_codec::detail::v1::TileContainer& container,
    const std::filesystem::path& path,
    const io::envelope::CompressionAlgorithm compression_algorithm = io::envelope::CompressionAlgorithm::None)
{
    REQUIRE(io::envelope::write<tile_codec::TileContainerSchema>(container, path, io::byte::WriteMode::Overwrite, true, compression_algorithm));
}

// Writes a tile container holding one subtile with the given schema.
template <typename Schema>
void write_single_subtile(const tile_codec::detail::v1::RasterTile& encoded, const std::filesystem::path& path)
{
    auto subtile = io::envelope::serialize<Schema>(encoded);
    REQUIRE(subtile);
    write_container({ { 0, subtile->size() }, std::move(*subtile) }, path);
}

template <typename PixelType>
raster_store::Tile<PixelType> patterned_tile(const unsigned side)
{
    raster_store::Tile<PixelType> tile(side);
    for (unsigned y = 0; y < side; ++y) {
        for (unsigned x = 0; x < side; ++x) {
            if constexpr (std::is_same_v<PixelType, float>) {
                tile.data.pixel({ x, y }) = float(x) * 0.5f - float(y);
            } else {
                tile.data.pixel({ x, y }) = glm::u8vec3(x % 256, y % 256, (x + 3 * y) % 256);
            }
            tile.attribution.pixel({ x, y }) = static_cast<std::uint16_t>(7 * x + y);
        }
    }
    return tile;
}

template <typename T>
radix::Raster<T> crop(const radix::Raster<T>& source, const radix::geometry::Aabb2ui& region)
{
    return raster::algorithm::copy(raster::make_view(source, glm::i64vec2(region.min), region.size()).value()).value();
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
    CHECK(std::ranges::all_of(tile.attribution, [](const auto value) { return value == 0; }));
    std::uint8_t value = 0;
    for (auto& byte : tile.data.bytes()) {
        byte = std::byte(value);
        value = static_cast<std::uint8_t>(value + 37);
    }
    tile.attribution.pixel({ 1, 1 }) = 65534;
    const auto path = directory.path() / "tile";
    TileCodec<TestType> writer({ 3, 3 });
    REQUIRE(writer.write(path, tile));
    auto read = writer.read(path);
    REQUIRE(read);
    check_bytes(read->data, tile.data);
    check_bytes(read->attribution, tile.attribution);

    TileCodec<TestType> uncompressed({ 3, 3 }, io::envelope::CompressionAlgorithm::None);
    const auto uncompressed_path = directory.path() / "uncompressed";
    REQUIRE(uncompressed.write(uncompressed_path, tile));
    read = writer.read(uncompressed_path);
    REQUIRE(read);
    check_bytes(read->data, tile.data);
}

TEST_CASE("AMORT never replaces an existing tile file", "[raster-store][codec]")
{
    TemporaryDirectory directory;
    const auto path = directory.path() / "tile";
    TileCodec<float> amort({ 3, 3 });
    raster_store::Tile<float> tile(3);
    tile.data.pixel({ 0, 0 }) = 1.0f;
    REQUIRE(amort.write(path, tile));
    const auto original = io::byte::read(path.string() + ".amort");
    REQUIRE(original);

    tile.data.pixel({ 0, 0 }) = 2.0f;
    const auto rejected = amort.write(path, tile);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == Error::Code::AlreadyExists);
    CHECK(io::byte::read(path.string() + ".amort").value() == original.value());
}

TEST_CASE("AMORT preserves NaN bits and data beneath NoData pixels", "[raster-store][codec]")
{
    TemporaryDirectory directory;
    raster_store::Tile<float> tile(3);
    tile.data.pixel({ 0, 0 }) = std::bit_cast<float>(std::uint32_t { 0x7fc01234 });
    tile.attribution.pixel({ 0, 0 }) = 1;
    tile.data.pixel({ 1, 0 }) = 42.0f;
    tile.data.pixel({ 2, 0 }) = std::bit_cast<float>(std::uint32_t { 0x80000000 });
    TileCodec<float> amort({ 3, 3 });
    REQUIRE(amort.write(directory.path() / "tile", tile));
    auto read = amort.read(directory.path() / "tile");
    REQUIRE(read);
    check_bytes(read->data, tile.data);
    check_bytes(read->attribution, tile.attribution);
}

TEST_CASE("AMORT validates dimensions and byte counts before allocating typed rasters", "[raster-store][codec]")
{
    TemporaryDirectory directory;
    const auto path = directory.path() / "tile";
    TileCodec<float> amort({ 3, 3 });
    raster_store::Tile<float> tile(3);
    tile.attribution = radix::Raster<std::uint16_t>(2);
    CHECK(amort.write(path, tile).error().code() == Error::Code::InvalidInput);
    TileCodec<float> rectangular({ 3, 2 });
    CHECK(rectangular.write(path, tile).error().code() == Error::Code::InvalidInput);

    tile_codec::detail::v1::RasterTile encoded { 3, 3, io::byte::vector(36), io::byte::vector(18) };
    SECTION("wrong dimensions") { encoded.width = 4; }
    SECTION("short data") { encoded.data.pop_back(); }
    SECTION("long attribution") { encoded.source_attribution.push_back(std::byte { 0 }); }
    write_single_subtile<tile_codec::TileSchema>(encoded, path.string() + ".amort");
    CHECK(amort.read(path).error().code() == Error::Code::CorruptData);
}

TEST_CASE("Image tile codecs store RGB8 data lossily and attribution exactly", "[raster-store][codec]")
{
    const std::string codec_name = GENERATE("amoit", "amojt");
    TemporaryDirectory directory;
    raster_store::Tile<glm::u8vec3> tile(32);
    for (unsigned y = 0; y < 32; ++y) {
        for (unsigned x = 0; x < 32; ++x) {
            tile.data.pixel({ x, y }) = glm::u8vec3(100 + 2 * x, 80 + 2 * y, 120);
            tile.attribution.pixel({ x, y }) = static_cast<std::uint16_t>(x + 32 * y);
        }
    }
    tile.attribution.pixel({ 31, 31 }) = 65534;
    const auto path = directory.path() / "tile";
    auto selected = tile_codec::from_name<glm::u8vec3>(codec_name, glm::uvec2(32));
    REQUIRE(selected);
    const auto& codec = **selected;
    CHECK(codec.paths(path) == std::vector { std::filesystem::path(path.string() + "." + codec_name) });
    const auto hash = codec.write(path, tile);
    REQUIRE(hash);
    CHECK(hash->size() >= store::min_hash_size);
    auto read = codec.read(path, *hash);
    REQUIRE(read);
    check_bytes(read->attribution, tile.attribution);
    REQUIRE(read->data.size() == tile.data.size());
    int max_error = 0;
    for (std::size_t i = 0; i < tile.data.buffer_length(); ++i) {
        const auto difference = glm::abs(glm::ivec3(read->data.buffer()[i]) - glm::ivec3(tile.data.buffer()[i]));
        max_error = std::max({ max_error, difference.x, difference.y, difference.z });
    }
    CHECK(max_error > 0);
    CHECK(max_error <= (codec_name == "amojt" ? 6 : 4));
    CHECK(codec.write(path, tile).error().code() == Error::Code::AlreadyExists);
    auto wrong_hash = *hash;
    wrong_hash.front() ^= std::byte { 1 };
    CHECK_FALSE(codec.read(path, wrong_hash));
    if (codec_name == "amojt") {
        const auto container = io::envelope::read<tile_codec::TileContainerSchema>(codec.paths(path).front());
        REQUIRE(container);
        const auto encoded = io::envelope::deserialize<tile_codec::JpegTileSchema>(container->data);
        REQUIRE(encoded);
        const auto jpeg = io::image::encode(tile.data, io::image::Format::Jpeg, { .jpeg_quality = 95 });
        REQUIRE(jpeg);
        CHECK(encoded->data == *jpeg);
    }
}

TEST_CASE("Image tile codecs validate images and attribution against the tile dimensions", "[raster-store][codec]")
{
    const std::string codec_name = GENERATE("amoit", "amojt");
    const auto format = codec_name == "amojt" ? io::image::Format::Jpeg : io::image::Format::JpegXl;
    TemporaryDirectory directory;
    const auto path = directory.path() / "tile";
    auto selected = tile_codec::from_name<glm::u8vec3>(codec_name, glm::uvec2(3));
    REQUIRE(selected);
    const auto& codec = **selected;
    tile_codec::detail::v1::RasterTile encoded { 3, 3, io::image::encode(io::image::RGB8(3), format).value(), io::byte::vector(18) };
    bool valid = false;
    SECTION("valid") { valid = true; }
    SECTION("wrong dimensions") { encoded.width = 4; }
    SECTION("image of other dimensions") { encoded.data = io::image::encode(io::image::RGB8(4), format).value(); }
    SECTION("truncated image") { encoded.data.resize(encoded.data.size() / 2); }
    SECTION("long attribution") { encoded.source_attribution.push_back(std::byte { 0 }); }
    const auto filename = codec.paths(path).front();
    if (codec_name == "amojt") {
        write_single_subtile<tile_codec::JpegTileSchema>(encoded, filename);
    } else {
        write_single_subtile<tile_codec::ImageTileSchema>(encoded, filename);
    }
    const auto read = codec.read(path);
    REQUIRE(read.has_value() == valid);
    if (!valid) {
        CHECK(read.error().code() == Error::Code::CorruptData);
    }
    // AMORT payloads use another envelope class.
    write_single_subtile<tile_codec::TileSchema>(encoded, filename);
    CHECK(codec.read(path).error().code() == Error::Code::CorruptData);
}

TEST_CASE("Image tile snapshots are available only for RGB8 payloads", "[raster-store][storage][codec]")
{
    const std::string codec_name = GENERATE("amoit", "amojt");
    CHECK(tile_codec::from_name<float>(codec_name, glm::uvec2(4)).error().code() == Error::Code::Unsupported);
    CHECK(tile_codec::from_name<glm::u8vec4>(codec_name, glm::uvec2(4)).error().code() == Error::Code::Unsupported);

    TemporaryDirectory directory;
    write_table(directory.path());
    auto options = small_options();
    options.codec_selector = codec_name;
    CHECK(storage::create<float>(directory.path() / "scalar", options).error().code() == Error::Code::Unsupported);
    const auto final = directory.path() / "colour";
    auto created_result = storage::create<glm::u8vec3>(final, options);
    REQUIRE(created_result);
    auto [created, created_metadata] = std::move(*created_result);
    raster_store::Tile<glm::u8vec3> tile(4);
    tile.data.fill(glm::u8vec3(255));
    tile.attribution.fill(1);
    REQUIRE(created->save({ 0, { 0, 0 } }, tile));
    REQUIRE(storage::publish(std::move(created)));
    CHECK(std::filesystem::exists(final / ("0/0/0." + codec_name)));
    auto opened = storage::open<glm::u8vec3>(final);
    REQUIRE(opened);
    auto [reader, metadata] = std::move(*opened);
    CHECK(metadata->codec_selector == codec_name);
    const auto loaded = reader->load({ 0, { 0, 0 } });
    REQUIRE(loaded);
    check_bytes(loaded->attribution, tile.attribution);
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
    auto encoded = manifest::encode_index(index, {});
    auto decoded = manifest::decode_index(encoded);
    REQUIRE(decoded);
    CHECK(decoded->index.is(store::NodeStatus::Inner, root).value());
    CHECK(decoded->index.is(store::NodeStatus::Virtual, { 1, { 0, 0 } }).value());
    CHECK(decoded->index.is(store::NodeStatus::Leaf, { 2, { 1, 1 } }).value());
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
    auto created_result = storage::create<float>(final, small_options());
    REQUIRE(created_result);
    auto [created, created_metadata] = std::move(*created_result);
    CHECK_FALSE(storage::open<float>(partial));
    CHECK_FALSE(storage::open<float>(partial / ""));
    CHECK(storage::open<float>(partial, { .allow_incomplete = true }));
    const auto metadata_before = io::byte::read(partial / manifest::metadata_file_name).value();
    raster_store::Tile<float> tile(4);
    tile.data.fill(12.5f);
    // Reference checks belong to RF builder, not storage reads or writes.
    tile.attribution.fill(65534);
    REQUIRE(created->save({ 0, { 0, 0 } }, tile));
    REQUIRE(created->save_index());
    CHECK(io::byte::read(partial / manifest::metadata_file_name).value() == metadata_before);
    auto checkpoint_result = storage::open<float>(partial, { .allow_incomplete = true });
    REQUIRE(checkpoint_result);
    auto [checkpoint, checkpoint_metadata] = std::move(*checkpoint_result);
    REQUIRE(checkpoint->load({ 0, { 0, 0 } }));
    CHECK(checkpoint->codec_selector().value() == "amort");
    auto metadata = manifest::read_metadata(partial);
    REQUIRE(metadata);
    CHECK(metadata->stored_tile_size == 4);
    CHECK(metadata->nominal_tile_size == 4);
    CHECK(metadata->payload_type == "float32");
    CHECK(metadata->codec_selector == "amort");
    REQUIRE(created_metadata);
    CHECK(created_metadata->stored_tile_size == metadata->stored_tile_size);
    CHECK(created_metadata->nominal_tile_size == metadata->nominal_tile_size);
    CHECK(created_metadata->layout_id == metadata->layout_id);
    CHECK(created_metadata->payload_type == metadata->payload_type);
    CHECK(created_metadata->codec_selector == metadata->codec_selector);

    REQUIRE(storage::publish(std::move(created)));
    CHECK_FALSE(std::filesystem::exists(partial));
    auto published_result = storage::open<float>(final);
    REQUIRE(published_result);
    auto [published, published_metadata] = std::move(*published_result);
    CHECK(published->base_path() == final);
    check_bytes(published->load({ 0, { 0, 0 } })->data, tile.data);
    CHECK(storage::open<std::uint32_t>(final).error().code() == Error::Code::Unsupported);
}

TEST_CASE("failed checkpoints retain the previous index and ignore unindexed files", "[raster-store][storage]")
{
    TemporaryDirectory directory;
    write_table(directory.path());
    auto created_result = storage::create<float>(directory.path() / "snapshot", small_options());
    REQUIRE(created_result);
    auto [created, created_metadata] = std::move(*created_result);
    const auto partial = created->base_path();
    REQUIRE(created->save({ 0, { 0, 0 } }, raster_store::Tile<float>(4)));
    std::filesystem::create_directory(partial / "raster_store.index.tmp");
    CHECK_FALSE(created->save_index());
    auto checkpoint_result = storage::open<float>(partial, { .allow_incomplete = true });
    REQUIRE(checkpoint_result);
    auto [checkpoint, checkpoint_metadata] = std::move(*checkpoint_result);
    CHECK(checkpoint->index().empty());
    CHECK(checkpoint->load({ 0, { 0, 0 } }).error().code() == Error::Code::NotFound);
    std::filesystem::remove(partial / "raster_store.index.tmp");
    REQUIRE(created->save_index());
    auto reopened_result = storage::open<float>(partial, { .allow_incomplete = true });
    REQUIRE(reopened_result);
    auto [reopened, reopened_metadata] = std::move(*reopened_result);
    CHECK(reopened->index().size() == 1);
}

TEST_CASE("raster publication does not scan payloads or reopen output", "[raster-store][storage]")
{
    TemporaryDirectory directory;
    write_table(directory.path());
    const auto final = directory.path() / "snapshot";
    auto created_result = storage::create<float>(final, small_options());
    REQUIRE(created_result);
    auto [created, created_metadata] = std::move(*created_result);
    REQUIRE(created->save({ 0, { 0, 0 } }, raster_store::Tile<float>(4)));
    REQUIRE(std::filesystem::remove(created->path_for({ 0, { 0, 0 } }).value()));
    REQUIRE(storage::publish(std::move(created)));
    auto opened_result = storage::open<float>(final);
    REQUIRE(opened_result);
    auto [opened, opened_metadata] = std::move(*opened_result);
    CHECK(opened->load({ 0, { 0, 0 } }).error().code() == Error::Code::NotFound);
}

TEST_CASE("raster reader selection follows metadata instead of file endings", "[raster-store][storage]")
{
    TemporaryDirectory directory;
    write_table(directory.path());
    const auto final = directory.path() / "snapshot";
    auto created_result = storage::create<float>(final, small_options());
    REQUIRE(created_result);
    auto [created, created_metadata] = std::move(*created_result);
    REQUIRE(created->save({ 0, { 0, 0 } }, raster_store::Tile<float>(4)));
    REQUIRE(storage::publish(std::move(created)));
    auto metadata = manifest::read_metadata(final).value();
    metadata.codec_selector = ".amort";
    REQUIRE(io::envelope::write<manifest::MetadataSchema>(metadata, final / manifest::metadata_file_name, io::byte::WriteMode::Overwrite));
    CHECK(storage::open<float>(final).error().code() == Error::Code::Unsupported);
    metadata.codec_selector = "amort";
    REQUIRE(io::envelope::write<manifest::MetadataSchema>(metadata, final / manifest::metadata_file_name, io::byte::WriteMode::Overwrite));
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
    auto created_result = storage::create<float>(rf, small_options());
    REQUIRE(created_result);
    auto [created, created_metadata] = std::move(*created_result);
    raster_store::Tile<float> tile(4);
    tile.data.fill(12.5f);
    REQUIRE(created->save({ 0, { 0, 0 } }, tile));
    REQUIRE(created->save({ 2, { 1, 1 } }, tile));
    REQUIRE(storage::publish(std::move(created)));
    {
        auto source_result = storage::open<float>(rf);
        REQUIRE(source_result);
        auto [source, source_metadata] = std::move(*source_result);
        auto options = small_options();
        options.copy_attribution_from_index = rf / manifest::index_file_name;
        auto target_result = storage::create<float>(tb, options);
        REQUIRE(target_result);
        auto [target, target_metadata] = std::move(*target_result);
        for (const auto key : { radix::tile::Id { 0, { 0, 0 } }, radix::tile::Id { 2, { 1, 1 } } }) {
            REQUIRE(target->link_from(key, *source));
            CHECK(std::filesystem::equivalent(source->path_for(key).value(), target->path_for(key).value()));
        }
        CHECK_FALSE(std::filesystem::equivalent(rf_root / attribution::file_name, target->base_path() / attribution::file_name));
        CHECK(io::byte::read(rf_root / attribution::file_name).value()
            == io::byte::read(target->base_path() / attribution::file_name).value());
        REQUIRE(storage::publish(std::move(target)));
    }
    std::filesystem::remove_all(rf_root);
    auto target_result = storage::open<float>(tb);
    REQUIRE(target_result);
    auto [target, target_metadata] = std::move(*target_result);
    CHECK(target->index().is(store::NodeStatus::Inner, { 0, { 0, 0 } }).value());
    CHECK(target->load({ 2, { 1, 1 } })->data.pixel({ 0, 0 }) == 12.5f);
    CHECK(attribution::read_table(tb / manifest::index_file_name));
}

TEST_CASE("byte writes report buffered device failures", "[io][bytes][raster-store]")
{
#ifdef __linux__
    const std::array bytes { std::byte { 42 } };
    auto written = io::byte::write(bytes, "/dev/full", io::byte::WriteMode::Overwrite, false);
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
    auto created_result = storage::create<float>(final, small_options());
    REQUIRE(created_result);
    auto [created, created_metadata] = std::move(*created_result);
    REQUIRE(storage::publish(std::move(created)));
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
        REQUIRE(io::envelope::write<manifest::MetadataSchema>(metadata, final / manifest::metadata_file_name, io::byte::WriteMode::Overwrite));
        CHECK(storage::open<float>(final).error().code() == Error::Code::Unsupported);
    }
    SECTION("rectangular metadata")
    {
        auto metadata = manifest::read_metadata(final).value();
        metadata.stored_tile_size = 2;
        REQUIRE(io::envelope::write<manifest::MetadataSchema>(metadata, final / manifest::metadata_file_name, io::byte::WriteMode::Overwrite));
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
    const auto bytes = io::byte::read(payload_path).value();
    auto envelope = io::envelope::detail::deserialize_from_bytes<io::envelope::Envelope>(bytes).value();
    auto expected_error = Error::Code::CorruptData;
    SECTION("hash") { envelope.compressed_data.back() ^= std::byte { 1 }; }
    SECTION("version")
    {
        envelope.header.class_version = 999;
        expected_error = Error::Code::Unsupported;
    }
    const auto modified = io::envelope::detail::serialize_to_bytes(envelope).value();
    REQUIRE(io::byte::write(modified, payload_path, io::byte::WriteMode::Overwrite));
    CHECK(amort.read(path).error().code() == expected_error);
}

TEST_CASE("abandoned raster output checkpoints but never publishes", "[raster-store][storage]")
{
    TemporaryDirectory directory;
    write_table(directory.path());
    const auto final = directory.path() / "snapshot";
    const auto partial = directory.path() / "snapshot.part";
    {
        auto created_result = storage::create<float>(final, small_options());
        REQUIRE(created_result);
        auto [created, created_metadata] = std::move(*created_result);
        REQUIRE(created->save({ 0, { 0, 0 } }, raster_store::Tile<float>(4)));
    }
    CHECK_FALSE(std::filesystem::exists(final));
    auto abandoned_result = storage::open<float>(partial, { .allow_incomplete = true });
    REQUIRE(abandoned_result);
    auto [abandoned, abandoned_metadata] = std::move(*abandoned_result);
    CHECK(abandoned->load({ 0, { 0, 0 } }));
}

TEMPLATE_TEST_CASE("snapshot mappings and stored halos round trip", "[raster-store]", float, glm::u8vec3, glm::u8vec4)
{
    TemporaryDirectory directory;
    write_table(directory.path());
    auto options = small_options();
    options.halo_width = 1;
    SECTION("type default") { }
    SECTION("explicit linear") { options.value_mapping = raster_store::pixel::Mapping::Linear; }
    SECTION("explicit srgba") { options.value_mapping = raster_store::pixel::Mapping::SRGBA; }
    auto created = storage::create<TestType>(directory.path() / "snapshot", options);
    REQUIRE(created);
    CHECK(created->second->nominal_tile_size == 4);
    CHECK(created->second->stored_tile_size == 6);
    CHECK(created->second->halo_width == 1);
    CHECK(created->second->value_mapping == options.value_mapping.value_or(raster_store::pixel::default_mapping<TestType>));
    raster_store::Tile<TestType> tile(6);
    tile.data.fill(TestType(7));
    REQUIRE(created->first->save({ 0, { 0, 0 } }, tile));
    REQUIRE(storage::publish(std::move(created->first)));
    auto opened = storage::open<TestType>(directory.path() / "snapshot");
    REQUIRE(opened);
    CHECK(opened->second->value_mapping == created->second->value_mapping);
    check_bytes(opened->first->load({ 0, { 0, 0 } })->data, tile.data);
}

TEST_CASE("snapshot geometry and mapping validation precede creation", "[raster-store]")
{
    TemporaryDirectory directory;
    write_table(directory.path());
    auto options = small_options();
    SECTION("nonpower interior") { options.nominal_tile_size = 3; }
    SECTION("zero interior") { options.nominal_tile_size = 0; }
    SECTION("excessive halo") { options.halo_width = 5; }
    SECTION("overflow")
    {
        options.nominal_tile_size = 1u << 31;
        options.halo_width = 1u << 31;
    }
    SECTION("unknown mapping") { options.value_mapping = static_cast<raster_store::pixel::Mapping>(99); }
    SECTION("area overlap") { options.overlap = 1; }
    SECTION("double vertex overlap")
    {
        options.placement = raster_store::pixel::Placement::Vertex;
        options.overlap = 2;
    }
    SECTION("unknown placement") { options.placement = static_cast<raster_store::pixel::Placement>(99); }
    CHECK_FALSE(storage::create<float>(directory.path() / "snapshot", options));
    CHECK_FALSE(std::filesystem::exists(directory.path() / "snapshot.part"));
}

TEST_CASE("vertex snapshots include the overlap in their stored tiles", "[raster-store][storage]")
{
    TemporaryDirectory directory;
    write_table(directory.path());
    auto options = small_options();
    options.halo_width = 1;
    options.placement = raster_store::pixel::Placement::Vertex;
    options.overlap = 1;
    auto created = storage::create<float>(directory.path() / "snapshot", options);
    REQUIRE(created);
    CHECK(created->second->nominal_tile_size == 4);
    CHECK(created->second->stored_tile_size == 7);
    raster_store::Tile<float> tile(7);
    tile.data.fill(3.5f);
    REQUIRE(created->first->save({ 0, { 0, 0 } }, tile));
    REQUIRE(storage::publish(std::move(created->first)));
    auto opened = storage::open<float>(directory.path() / "snapshot");
    REQUIRE(opened);
    CHECK(opened->second->placement == raster_store::pixel::Placement::Vertex);
    CHECK(opened->second->overlap == 1);
    CHECK(opened->second->halo_width == 1);
    check_bytes(opened->first->load({ 0, { 0, 0 } })->data, tile.data);
}

TEST_CASE("raster metadata round-trips both placements as version 2", "[raster-store]")
{
    TemporaryDirectory directory;
    manifest::Metadata metadata {
        "zoom_x_y_google", "float32", "amort", 4, 4, 0, raster_store::pixel::Mapping::Linear, raster_store::pixel::Placement::Area, 0
    };
    SECTION("area") { }
    SECTION("vertex without overlap") { metadata.placement = raster_store::pixel::Placement::Vertex; }
    SECTION("vertex with overlap and halo")
    {
        metadata.placement = raster_store::pixel::Placement::Vertex;
        metadata.overlap = 1;
        metadata.halo_width = 2;
        metadata.stored_tile_size = 9;
    }
    REQUIRE(manifest::write_metadata(metadata, directory.path()));
    const auto header = io::envelope::read_header(directory.path() / manifest::metadata_file_name);
    REQUIRE(header);
    CHECK(header->class_version == 2);
    const auto read = manifest::read_metadata(directory.path());
    REQUIRE(read);
    CHECK(read->placement == metadata.placement);
    CHECK(read->overlap == metadata.overlap);
    CHECK(read->halo_width == metadata.halo_width);
    CHECK(read->stored_tile_size == metadata.stored_tile_size);
}

TEST_CASE("version 1 raster metadata reads as area placement without overlap", "[raster-store]")
{
    TemporaryDirectory directory;
    const manifest::detail::v1::Metadata previous { "zoom_x_y_google", "vec3<uint8>", "amoit", 6, 4, 1, raster_store::pixel::Mapping::SRGBA };
    const auto bytes = io::envelope::serialize<manifest::MetadataSchema, 1>(previous);
    REQUIRE(bytes);
    REQUIRE(io::byte::write(*bytes, directory.path() / manifest::metadata_file_name, io::byte::WriteMode::Overwrite));
    const auto metadata = manifest::read_metadata(directory.path());
    REQUIRE(metadata);
    CHECK(metadata->layout_id == previous.layout_id);
    CHECK(metadata->payload_type == previous.payload_type);
    CHECK(metadata->codec_selector == previous.codec_selector);
    CHECK(metadata->stored_tile_size == 6);
    CHECK(metadata->nominal_tile_size == 4);
    CHECK(metadata->halo_width == 1);
    CHECK(metadata->value_mapping == raster_store::pixel::Mapping::SRGBA);
    CHECK(metadata->placement == raster_store::pixel::Placement::Area);
    CHECK(metadata->overlap == 0);
}

TEST_CASE("raster metadata permits overlap only for vertex placement", "[raster-store]")
{
    manifest::Metadata metadata {
        "zoom_x_y_google", "float32", "amort", 5, 4, 0, raster_store::pixel::Mapping::Linear, raster_store::pixel::Placement::Vertex, 1
    };
    REQUIRE(manifest::validate(metadata));
    SECTION("area overlap") { metadata.placement = raster_store::pixel::Placement::Area; }
    SECTION("double overlap")
    {
        metadata.overlap = 2;
        metadata.stored_tile_size = 6;
    }
    SECTION("stored size without overlap") { metadata.stored_tile_size = 4; }
    SECTION("unknown placement") { metadata.placement = static_cast<raster_store::pixel::Placement>(2); }
    CHECK(manifest::validate(metadata).error().code() == Error::Code::InvalidInput);
}

TEST_CASE("AMORT tiles return and verify their payload hash", "[raster-store][codec][hash]")
{
    TemporaryDirectory directory;
    const auto path = directory.path() / "tile";
    raster_store::Tile<float> tile(3);
    tile.data.fill(2.5f);
    const TileCodec<float> amort({ 3, 3 });
    const auto hash = amort.write(path, tile);
    REQUIRE(hash);
    CHECK(hash->size() == 8);
    CHECK(*hash == io::envelope::read_header(path.string() + ".amort")->hash);
    CHECK(amort.read(path, *hash));

    auto different = *hash;
    different.front() ^= std::byte { 1 };
    CHECK(amort.read(path, different).error().code() == Error::Code::CorruptData);
}

TEST_CASE("partial tile reads equal crops of full reads", "[raster-store][codec][partial]")
{
    const std::string codec_name = GENERATE("amort", "amoft", "amoit", "amojt");
    CAPTURE(codec_name);
    TemporaryDirectory directory;
    const auto path = directory.path() / "tile";
    // Three subtiles per axis, the last one pixel wide.
    constexpr unsigned side = 2 * tile_codec::subtile_side + 1;
    const std::vector<raster_store::Tile<float>::Part> regions {
        { { 10, 20 }, { 30, 40 } }, // Inside one subtile.
        { { 200, 20 }, { 300, 40 } }, // Across a vertical subtile edge.
        { { 250, 250 }, { 262, 262 } }, // Across both subtile edges.
        { { 512, 0 }, { 513, 513 } }, // The ragged last column.
        { { 500, 505 }, { 513, 513 } }, // Into the ragged corner.
        { { 0, 0 }, { 513, 513 } },
    };
    const auto check = [&]<typename PixelType>(const raster_store::Tile<PixelType>& tile) {
        const auto selected = tile_codec::from_name<PixelType>(codec_name, glm::uvec2(side));
        REQUIRE(selected);
        const auto& codec = **selected;
        const auto hash = codec.write(path, tile);
        REQUIRE(hash);
        const auto full = codec.read(path, *hash);
        REQUIRE(full);
        check_bytes(full->attribution, tile.attribution);
        if constexpr (std::is_same_v<PixelType, float>) {
            check_bytes(full->data, tile.data);
        }
        for (const auto& region : regions) {
            CAPTURE(region.min.x, region.min.y, region.max.x, region.max.y);
            const auto part = codec.read_part(path, region, *hash);
            REQUIRE(part);
            check_bytes(part->data, crop(full->data, region));
            check_bytes(part->attribution, crop(full->attribution, region));
        }
    };
    if (codec_name == "amort" || codec_name == "amoft") {
        check(patterned_tile<float>(side));
    } else {
        check(patterned_tile<glm::u8vec3>(side));
    }
}

TEST_CASE("partial tile reads reject invalid regions, hashes and containers", "[raster-store][codec][partial]")
{
    TemporaryDirectory directory;
    const auto path = directory.path() / "tile";
    const auto file = path.string() + ".amort";
    // Two subtiles per axis, stored uncompressed so that flipping a byte only changes the hash.
    constexpr unsigned side = tile_codec::subtile_side + 44;
    const TileCodec<float> amort(glm::uvec2(side), io::envelope::CompressionAlgorithm::None);
    const auto hash = amort.write(path, patterned_tile<float>(side));
    REQUIRE(hash);
    const raster_store::Tile<float>::Part first_subtile { { 0, 0 }, { 10, 10 } };
    const raster_store::Tile<float>::Part last_subtile { { 290, 290 }, { side, side } };
    REQUIRE(amort.read_part(path, first_subtile, *hash));

    SECTION("invalid regions")
    {
        CHECK(amort.read_part(path, { { 5, 5 }, { 5, 10 } }).error().code() == Error::Code::InvalidInput);
        CHECK(amort.read_part(path, { { 0, 0 }, { side + 1, 1 } }).error().code() == Error::Code::InvalidInput);
        CHECK(amort.read_part(path, {}).error().code() == Error::Code::InvalidInput);
    }

    SECTION("a different expected hash")
    {
        auto different = *hash;
        different.front() ^= std::byte { 1 };
        CHECK(amort.read_part(path, first_subtile, different).error().code() == Error::Code::CorruptData);
    }

    SECTION("a corrupt subtile fails only reads that cover it")
    {
        // The last byte belongs to the last subtile; the container header and its hash stay unchanged.
        auto bytes = io::byte::read(file).value();
        bytes.back() ^= std::byte { 1 };
        REQUIRE(io::byte::write(bytes, file, io::byte::WriteMode::Overwrite));
        CHECK(amort.read_part(path, first_subtile, *hash));
        CHECK(amort.read_part(path, last_subtile, *hash).error().code() == Error::Code::CorruptData);
        CHECK(amort.read(path, *hash).error().code() == Error::Code::CorruptData);
    }

    SECTION("a mismatched uncompressed size")
    {
        auto envelope = io::envelope::detail::deserialize_from_bytes<io::envelope::Envelope>(io::byte::read(file).value()).value();
        ++envelope.header.uncompressed_size;
        REQUIRE(io::byte::write(io::envelope::detail::serialize_to_bytes(envelope).value(), file, io::byte::WriteMode::Overwrite));
        CHECK(amort.read_part(path, first_subtile, *hash).error().code() == Error::Code::CorruptData);
        CHECK(amort.read(path, *hash).error().code() == Error::Code::CorruptData);
    }

    SECTION("corrupt subtile offsets")
    {
        auto container = io::envelope::read<tile_codec::TileContainerSchema>(file).value();
        auto& offsets = container.subtile_offsets;
        SECTION("a missing offset") { offsets.pop_back(); }
        SECTION("a nonzero first offset") { offsets.front() = 1; }
        SECTION("decreasing offsets") { std::swap(offsets[1], offsets[2]); }
        SECTION("an offset past the data") { offsets.back() += 1; }
        write_container(container, file);
        CHECK(amort.read(path).error().code() == Error::Code::CorruptData);
        CHECK(amort.read_part(path, first_subtile).error().code() == Error::Code::CorruptData);
    }

    SECTION("truncated files")
    {
        auto bytes = io::byte::read(file).value();
        SECTION("in the header") { bytes.resize(20); }
        SECTION("in the last subtile") { bytes.pop_back(); }
        REQUIRE(io::byte::write(bytes, file, io::byte::WriteMode::Overwrite));
        CHECK(amort.read(path).error().code() == Error::Code::CorruptData);
        CHECK(amort.read_part(path, first_subtile).error().code() == Error::Code::CorruptData);
    }

    SECTION("an old single-envelope tile")
    {
        const tile_codec::detail::v1::RasterTile encoded { side, side, io::byte::vector(side * side * 4), io::byte::vector(side * side * 2) };
        REQUIRE(io::envelope::write<tile_codec::TileSchema>(encoded, file, io::byte::WriteMode::Overwrite));
        CHECK(amort.read(path).error().code() == Error::Code::CorruptData);
        CHECK(amort.read_part(path, first_subtile).error().code() == Error::Code::CorruptData);
    }

    SECTION("a compressed container")
    {
        write_container(io::envelope::read<tile_codec::TileContainerSchema>(file).value(), file, io::envelope::CompressionAlgorithm::ZstdDefaultCompression);
        CHECK(amort.read(path));
        CHECK(amort.read_part(path, first_subtile).error().code() == Error::Code::Unsupported);
    }
}

TEST_CASE("tile codecs reject dimensions whose subtile offsets a container cannot hold", "[raster-store][codec][partial]")
{
    TemporaryDirectory directory;
    const auto path = directory.path() / "tile";
    // The subtile count of the largest side must not wrap around to zero.
    const TileCodec<float> amort(glm::uvec2(std::numeric_limits<unsigned>::max()));
    write_container({ { 0 }, {} }, path.string() + ".amort");
    CHECK(amort.read_part(path, { { 0, 0 }, { 1, 1 } }).error().code() == Error::Code::Unsupported);
    CHECK(amort.read(path).error().code() == Error::Code::Unsupported);
    CHECK(amort.write(directory.path() / "other", raster_store::Tile<float>(1)).error().code() == Error::Code::Unsupported);
}

TEST_CASE("raster index adapters round-trip sorted tile hashes and reject duplicate and invalid keys", "[raster-store][index][hash]")
{
    store::Index<raster_store::StoreTraits> index;
    const radix::tile::Id first { 1, { 1, 0 } };
    const radix::tile::Id second { 1, { 0, 1 } };
    REQUIRE(index.add(first));
    REQUIRE(index.add(second));
    store::Key2Hash_Map<raster_store::StoreTraits> hashes { { second, store::NodeDataHash(8, std::byte { 2 }) },
        { first, store::NodeDataHash(5, std::byte { 1 }) } };
    auto encoded = manifest::encode_index(index, hashes);
    REQUIRE(encoded.hashes.size() == 2);
    CHECK(encoded.hashes[0].id == std::min(first, second));
    CHECK(encoded.hashes[1].id == std::max(first, second));
    const auto decoded = manifest::decode_index(encoded);
    REQUIRE(decoded);
    CHECK(decoded->hashes == hashes);

    SECTION("duplicate key") { encoded.hashes.push_back(encoded.hashes.front()); }
    SECTION("invalid key") { encoded.hashes.front().id.coords.x = 2; }
    CHECK(manifest::decode_index(encoded).error().code() == Error::Code::CorruptData);
}

TEST_CASE("raster snapshots checkpoint tile hashes and verify loads against them", "[raster-store][storage][hash]")
{
    TemporaryDirectory directory;
    write_table(directory.path());
    const auto final = directory.path() / "snapshot";
    const radix::tile::Id key { 0, { 0, 0 } };
    raster_store::Tile<float> tile(4);
    tile.data.fill(1.5f);
    {
        auto created = storage::create<float>(final, small_options());
        REQUIRE(created);
        REQUIRE(created->first->save(key, tile));
        CHECK(created->first->hash_for(key).value() == io::envelope::read_header(created->first->path_for(key).value())->hash);
        REQUIRE(storage::publish(std::move(created->first)));
    }

    auto opened = storage::open<float>(final);
    REQUIRE(opened);
    const auto path = opened->first->path_for(key).value();
    CHECK(opened->first->hash_for(key).value() == io::envelope::read_header(path)->hash);
    const raster_store::Tile<float>::Part region { { 1, 1 }, { 3, 2 } };
    const auto part = opened->first->load_part(key, region);
    REQUIRE(part);
    CHECK(part->data.size() == region.size());
    CHECK(std::ranges::all_of(part->data, [](const float value) { return value == 1.5f; }));

    tile.data.fill(2.5f);
    REQUIRE(std::filesystem::remove(path));
    REQUIRE(TileCodec<float>({ 4, 4 }).write(path.parent_path() / path.stem(), tile));
    CHECK(opened->first->load(key).error().code() == Error::Code::CorruptData);
    CHECK(opened->first->load_part(key, region).error().code() == Error::Code::CorruptData);
}

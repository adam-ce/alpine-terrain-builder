#include "merge.h"
#include "../temporary_directory.h"
#include "inputs.h"
#include "io/bytes.h"
#include "partition.h"
#include "priorities.h"
#include "raster_store/read_tile_with_halo.h"
#include "raster_store/scaler.h"
#include "raster_store/storage.h"
#include "selection.h"
#include "statistics.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <functional>
#include <set>

namespace {
using Key = radix::tile::Id;
namespace storage = raster_store::storage;
namespace merge = rf_merger::merge;
namespace statistics = rf_merger::statistics;
using Tile = raster_store::Tile<float>;
using Colour = raster_store::Tile<glm::u8vec3>;
constexpr unsigned side = 64;

void write_text(const std::filesystem::path& path, const std::string& text)
{
    REQUIRE(io::write_bytes_to_path(std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()), path));
}

template <typename PixelType = float>
raster_store::Tile<PixelType> tile(PixelType value, std::uint16_t attribution)
{
    raster_store::Tile<PixelType> result(side);
    result.data.fill(value);
    result.source_attribution.fill(attribution);
    return result;
}

struct Fixture {
    test::TemporaryDirectory directory { "rf-merger" };

    Fixture()
    {
        const std::string entity
            = R"({"spatial_resolution":1,"acquisition_date":"2026","ingestion_date":"today","copyright":"test","copyright_link":"https://example.org","license":"test"})";
        std::string table = "[" + entity;
        for (unsigned i = 1; i < 16; ++i) {
            table += "," + entity;
        }
        write_text(directory.path() / raster_store::attribution::file_name, table + "]");
    }

    std::filesystem::path path(const std::string& name) const { return directory.path() / name; }

    template <typename PixelType = float>
    void snapshot(const std::string& name, const std::vector<std::pair<Key, raster_store::Tile<PixelType>>>& tiles, storage::CreateOptions options = {})
    {
        options.nominal_tile_size = tiles.empty() ? side : tiles.front().second.data.width();
        auto created = storage::create<PixelType>(path(name), options);
        REQUIRE(created);
        for (const auto& [key, value] : tiles) {
            REQUIRE(created->first->save(key, value));
        }
        REQUIRE(storage::publish(std::move(created->first)));
    }

    merge::Options options(const std::string& left, const std::string& right, const std::string& priorities, const std::string& output) const
    {
        write_text(path(output + ".priorities.json"), priorities);
        merge::Options result;
        result.left = path(left);
        result.right = path(right);
        result.priorities = path(output + ".priorities.json");
        result.output = path(output);
        return result;
    }

    merge::Report merge(const std::string& left, const std::string& right, const std::string& priorities, const std::string& output) const
    {
        return merge::run(options(left, right, priorities, output));
    }

    template <typename PixelType = float>
    auto open(const std::string& name) const
    {
        auto opened = storage::open<PixelType>(path(name));
        REQUIRE(opened);
        return std::move(opened->first);
    }
};

template <typename Storage>
std::set<Key> physical(const Storage& storage)
{
    std::set<Key> result;
    for (const auto& [key, status] : storage.index()) {
        if (status == store::NodeStatus::Leaf || status == store::NodeStatus::Inner) {
            result.insert(key);
        }
    }
    return result;
}

// The code of the Error::Exception thrown by operation.
Error::Code thrown(const std::function<void()>& operation)
{
    try {
        operation();
    } catch (const Error::Exception& exception) {
        return exception.error().code();
    }
    FAIL("no Error::Exception thrown");
    return Error::Code::Internal;
}

template <typename T>
bool same(const radix::Raster<T>& first, const radix::Raster<T>& second)
{
    return first.size() == second.size() && std::ranges::equal(first.buffer(), second.buffer());
}

template <typename Storage>
bool linked(const Storage& input, const Storage& output, const Key& key)
{
    return std::filesystem::equivalent(*input.path_for(key), *output.path_for(key));
}

// Keys covering the coarse tile's region once it is split around descendant.
std::set<Key> split(const Key& coarse, const Key& descendant)
{
    std::set<Key> result;
    for (Key current = descendant; current != coarse; current = current.parent()) {
        for (const auto& sibling : current.parent().children()) {
            result.insert(sibling);
        }
    }
    for (Key current = descendant.parent(); current != coarse; current = current.parent()) {
        result.erase(current);
    }
    return result;
}
} // namespace

TEST_CASE("RF merger priority tables are JSON arrays of attribution indices", "[rf-merger]")
{
    using rf_merger::priorities::parse;
    CHECK(parse("[]") == std::vector<std::uint16_t> {});
    CHECK(parse(" [ 7 , 3,12 ]\n") == std::vector<std::uint16_t> { 7, 3, 12 });
    CHECK(parse("[65534]") == std::vector<std::uint16_t> { 65534 });
    // GDAL's parser also accepts trailing commas, trailing data and leading zeros.
    for (const auto* invalid : { "", "[", "[,1]", "{}", "[1.5]", "[-1]", "[\"1\"]", "[1 2]" }) {
        INFO(invalid);
        CHECK_FALSE(parse(invalid));
    }
    for (const auto* rejected : { "[0]", "[65535]", "[99999999999999999999]", "[3,4,3]" }) {
        INFO(rejected);
        const auto result = parse(rejected);
        REQUIRE_FALSE(result);
        CHECK(result.error().code() == Error::Code::InvalidInput);
    }
    const auto ranks = rf_merger::priorities::ranks({ 7, 3 });
    CHECK(ranks[0] == 0);
    CHECK(ranks[5] == 1);
    CHECK(ranks[3] == 2);
    CHECK(ranks[7] == 3);
}

TEST_CASE("RF merger selection prefers attribution, priority, zoom and the right input", "[rf-merger]")
{
    using rf_merger::selection::Side;
    using rf_merger::selection::whole_tile;
    CHECK(whole_tile({ true, 2 }, { false, 9 }) == Side::Left);
    CHECK(whole_tile({ false, 9 }, { true, 2 }) == Side::Right);
    CHECK(whole_tile({ false, 9 }, { false, 2 }) == Side::Left);
    CHECK(whole_tile({ false, 2 }, { false, 2 }) == Side::Right);
    CHECK_FALSE(whole_tile({ true, 2 }, { true, 3 }));

    const auto ranks = rf_merger::priorities::ranks({ 7, 3 });
    const rf_merger::selection::Pixel coarse_right(ranks, 5, 2);
    CHECK(coarse_right.right_wins(0, 4));
    CHECK_FALSE(coarse_right.right_wins(4, 0));
    CHECK_FALSE(coarse_right.right_wins(7, 3));
    CHECK(coarse_right.right_wins(3, 7));
    CHECK(coarse_right.right_wins(5, 3));
    CHECK_FALSE(coarse_right.right_wins(5, 6));
    CHECK_FALSE(coarse_right.right_wins(3, 3));
    CHECK_FALSE(coarse_right.right_wins(0, 0));
    const rf_merger::selection::Pixel equal(ranks, 4, 4);
    CHECK(equal.right_wins(0, 0));
    CHECK(equal.right_wins(5, 6));
}

TEST_CASE("RF merger partition follows input topology without payload reads", "[rf-merger]")
{
    using rf_merger::partition::Cursor;
    using rf_merger::partition::Index;
    using rf_merger::partition::Leaf;
    const auto collect = [](const Index& left, const Index& right) {
        std::vector<Leaf> leaves;
        Cursor cursor(left, right);
        while (const auto leaf = cursor.next()) {
            leaves.push_back(*leaf);
        }
        return leaves;
    };
    Index left, right;
    CHECK(collect(left, right).empty());

    const Key coarse { 1, { 0, 0 } };
    const Key fine { 3, { 1, 2 } };
    const Key disjoint { 2, { 3, 3 } };
    REQUIRE(left.add(coarse));
    REQUIRE(right.add(fine));
    REQUIRE(right.add(disjoint));
    const auto leaves = collect(left, right);
    std::set<Key> keys;
    for (const auto& leaf : leaves) {
        INFO(to_string(leaf.key));
        CHECK(keys.insert(leaf.key).second);
        CHECK(leaf.left == (leaf.key == disjoint ? std::nullopt : std::optional(coarse)));
        CHECK(leaf.right == (leaf.key == fine ? std::optional(fine) : leaf.key == disjoint ? std::optional(disjoint) : std::nullopt));
        CHECK(rf_merger::partition::is_leaf(left, right, leaf.key));
    }
    auto expected = split(coarse, fine);
    expected.insert(disjoint);
    CHECK(keys == expected);
    CHECK_FALSE(rf_merger::partition::is_leaf(left, right, coarse));
    CHECK_FALSE(rf_merger::partition::is_leaf(left, right, { 1, { 1, 0 } }));
}

TEST_CASE("RF merger hard-links disjoint input tiles and publishes statistics", "[rf-merger]")
{
    Fixture f;
    const Key a { 1, { 0, 0 } };
    const Key b { 1, { 1, 1 } };
    f.snapshot("left", { { a, tile(1.f, 1) } });
    f.snapshot("right", { { b, tile(2.f, 2) } });
    const auto report = f.merge("left", "right", "[]", "merged");
    CHECK(report.statistics_complete);
    CHECK(report.statistics.linked_left.tiles == 1);
    CHECK(report.statistics.linked_right.tiles == 1);
    CHECK(report.statistics.total().tiles == 2);
    auto merged = f.open("merged");
    CHECK(physical(*merged) == std::set<Key> { a, b });
    CHECK(linked(*f.open("left"), *merged, a));
    CHECK(linked(*f.open("right"), *merged, b));
    CHECK(report.statistics.linked_left.bytes == std::filesystem::file_size(*merged->path_for(a)));
    CHECK_FALSE(std::filesystem::exists(merged->base_path() / "inputs.tmp"));
    CHECK_FALSE(std::filesystem::exists(merged->base_path() / "statistics.tmp"));
    CHECK_FALSE(std::filesystem::exists(f.path("merged.part")));
}

TEST_CASE("RF merger selects attributed pixels by priority at equal zoom", "[rf-merger]")
{
    Fixture f;
    const Key key { 2, { 1, 2 } };
    auto left = tile(1.f, 7);
    for (unsigned y = 0; y < side; ++y) {
        for (unsigned x = side / 2; x < side; ++x) {
            left.source_attribution.pixel({ x, y }) = 0;
        }
    }
    f.snapshot("left", { { key, left } });
    f.snapshot("right", { { key, tile(2.f, 3) } });

    SECTION("higher-priority right input wins every pixel and is linked")
    {
        const auto report = f.merge("left", "right", "[3, 7]", "merged");
        CHECK(report.statistics.linked_right.tiles == 1);
        CHECK(linked(*f.open("right"), *f.open("merged"), key));
    }
    SECTION("mixed selection prefers priority and nonzero attribution")
    {
        const auto report = f.merge("left", "right", "[7, 3]", "merged");
        CHECK(report.statistics.mixed.tiles == 1);
        auto merged = f.open("merged");
        auto loaded = merged->load(key);
        REQUIRE(loaded);
        CHECK(loaded->data.pixel({ 0, 5 }) == 1.f);
        CHECK(loaded->source_attribution.pixel({ 0, 5 }) == 7);
        CHECK(loaded->data.pixel({ side - 1, 5 }) == 2.f);
        CHECK(loaded->source_attribution.pixel({ side - 1, 5 }) == 3);
        CHECK_FALSE(linked(*f.open("right"), *merged, key));
    }
    SECTION("unlisted attributions tie and the right input wins")
    {
        const auto report = f.merge("left", "right", "[]", "merged");
        CHECK(report.statistics.linked_right.tiles == 1);
    }
}

TEST_CASE("RF merger refines by topology and resamples coarse winners", "[rf-merger]")
{
    Fixture f;
    const Key coarse { 1, { 0, 0 } };
    const Key fine { 3, { 1, 2 } };
    const auto expected = split(coarse, fine);

    SECTION("attributed coarse tile wins as a whole over fine support")
    {
        f.snapshot("left", { { coarse, tile(5.f, 4) } });
        f.snapshot("right", { { fine, tile(9.f, 0) } });
        const auto report = f.merge("left", "right", "[]", "merged");
        auto merged = f.open("merged");
        CHECK(physical(*merged) == expected);
        CHECK(report.statistics.left_only.tiles == expected.size());
        CHECK(report.statistics.total().tiles == expected.size());
        auto loaded = merged->load(fine);
        REQUIRE(loaded);
        CHECK(loaded->data.pixel({ 10, 20 }) == Catch::Approx(5.f));
        CHECK(loaded->source_attribution.pixel({ 10, 20 }) == 4);
    }
    SECTION("fully overridden fine tile still refines the output")
    {
        f.snapshot("left", { { coarse, tile(5.f, 4) } });
        f.snapshot("right", { { fine, tile(9.f, 6) } });
        const auto report = f.merge("left", "right", "[4, 6]", "merged");
        CHECK(physical(*f.open("merged")) == expected);
        CHECK(report.statistics.left_only.tiles == expected.size());
        CHECK(report.statistics.mixed.tiles == 0);
    }
    SECTION("attributed fine tile beats unattributed coarse support and is linked")
    {
        f.snapshot("left", { { coarse, tile(5.f, 0) } });
        f.snapshot("right", { { fine, tile(9.f, 6) } });
        const auto report = f.merge("left", "right", "[]", "merged");
        auto merged = f.open("merged");
        CHECK(physical(*merged) == expected);
        CHECK(report.statistics.linked_right.tiles == 1);
        CHECK(report.statistics.left_only.tiles == expected.size() - 1);
        CHECK(linked(*f.open("right"), *merged, fine));
        auto sibling = merged->load({ 3, { 0, 2 } });
        REQUIRE(sibling);
        CHECK(sibling->data.pixel({ 0, 0 }) == Catch::Approx(5.f));
        CHECK(sibling->source_attribution.pixel({ 0, 0 }) == 0);
    }
    SECTION("fine pixels fill coarse attribution holes")
    {
        auto holed = tile(5.f, 4);
        holed.source_attribution.fill(0);
        holed.source_attribution.pixel({ 0, 0 }) = 4;
        f.snapshot("left", { { coarse, holed } });
        f.snapshot("right", { { fine, tile(9.f, 6) } });
        const auto report = f.merge("left", "right", "[4]", "merged");
        CHECK(report.statistics.linked_right.tiles == 1);
        CHECK(linked(*f.open("right"), *f.open("merged"), fine));
    }
    SECTION("a single winning fine pixel produces a mixed tile")
    {
        f.snapshot("left", { { coarse, tile(5.f, 4) } });
        auto winner = tile(9.f, 6);
        winner.source_attribution.pixel({ 3, 4 }) = 8;
        f.snapshot("right", { { fine, winner } });
        const auto report = f.merge("left", "right", "[8, 4, 6]", "merged");
        CHECK(report.statistics.mixed.tiles == 1);
        auto loaded = f.open("merged")->load(fine);
        REQUIRE(loaded);
        CHECK(loaded->data.pixel({ 3, 4 }) == 9.f);
        CHECK(loaded->source_attribution.pixel({ 3, 4 }) == 8);
        CHECK(loaded->data.pixel({ 4, 4 }) == Catch::Approx(5.f));
        CHECK(loaded->source_attribution.pixel({ 4, 4 }) == 4);
    }
    SECTION("unattributed inputs prefer the finer tile")
    {
        f.snapshot("left", { { coarse, tile(5.f, 0) } });
        f.snapshot("right", { { fine, tile(9.f, 0) } });
        const auto report = f.merge("left", "right", "[]", "merged");
        CHECK(report.statistics.linked_right.tiles == 1);
        CHECK(report.statistics.left_only.tiles == expected.size() - 1);
    }
}

TEST_CASE("RF merger upscaling agrees with the halo reader and paired scaler", "[rf-merger]")
{
    Fixture f;
    const Key coarse { 1, { 0, 0 } };
    const Key neighbour { 1, { 1, 0 } };
    const Key fine { 3, { 0, 0 } };
    auto gradient = tile(0.f, 4);
    for (unsigned y = 0; y < side; ++y) {
        for (unsigned x = 0; x < side; ++x) {
            gradient.data.pixel({ x, y }) = float(x) + 100.f * float(y);
        }
    }
    f.snapshot("left", { { coarse, gradient }, { neighbour, tile(1000.f, 4) } });
    f.snapshot("right", { { fine, tile(0.f, 0) }, { { 3, { 3, 1 } }, tile(0.f, 0) } });
    f.merge("left", "right", "[]", "merged");
    auto merged = f.open("merged");
    auto left = f.open("left");
    const auto metadata = raster_store::io::manifest::read_metadata(f.path("left"));
    REQUIRE(metadata);
    for (const Key leaf : { fine, Key { 3, { 3, 1 } }, Key { 2, { 1, 1 } } }) {
        INFO(to_string(leaf));
        auto halo = raster_store::read_tile_with_halo(*left, *metadata, coarse, 3, raster::algorithm::Resampling::Lanczos3);
        REQUIRE(halo);
        const unsigned levels = leaf.zoom_level - coarse.zoom_level;
        const auto offset = glm::ivec2(leaf.coords & glm::uvec2((1u << levels) - 1)) * int(side);
        auto direct = raster_store::scaler::scale(halo->data,
            halo->source_attribution,
            3,
            int(levels),
            raster::algorithm::Resampling::Lanczos3,
            offset,
            glm::uvec2(side),
            raster_store::pixel::Mapping::Linear);
        REQUIRE(direct);
        auto loaded = merged->load(leaf);
        REQUIRE(loaded);
        CHECK(same(loaded->data, direct->first));
        CHECK(same(loaded->source_attribution, direct->second));
    }
}

TEST_CASE("RF merger merges RGB8 inputs", "[rf-merger]")
{
    Fixture f;
    const Key key { 2, { 0, 0 } };
    auto left = tile(glm::u8vec3(10, 20, 30), 1);
    left.source_attribution.pixel({ 1, 1 }) = 0;
    f.snapshot<glm::u8vec3>("left", { { key, left } });
    f.snapshot<glm::u8vec3>("right", { { key, tile(glm::u8vec3(200, 100, 50), 2) } });
    const auto report = f.merge("left", "right", "[1]", "merged");
    CHECK(report.statistics.mixed.tiles == 1);
    auto loaded = f.open<glm::u8vec3>("merged")->load(key);
    REQUIRE(loaded);
    CHECK(loaded->data.pixel({ 0, 0 }) == glm::u8vec3(10, 20, 30));
    CHECK(loaded->data.pixel({ 1, 1 }) == glm::u8vec3(200, 100, 50));
}

TEST_CASE("RF merger rejects incompatible inputs and destinations", "[rf-merger]")
{
    Fixture f;
    const Key key { 1, { 0, 0 } };
    f.snapshot("left", { { key, tile(1.f, 1) } });

    SECTION("dimensions")
    {
        raster_store::Tile<float> large(128);
        f.snapshot("right", { { key, large } });
        CHECK(thrown([&] { f.merge("left", "right", "[]", "merged"); }) == Error::Code::InvalidInput);
    }
    SECTION("value mapping")
    {
        storage::CreateOptions options;
        options.value_mapping = raster_store::pixel::Mapping::SRGBA;
        f.snapshot("right", { { key, tile(1.f, 1) } }, options);
        CHECK(thrown([&] { f.merge("left", "right", "[]", "merged"); }) == Error::Code::InvalidInput);
    }
    SECTION("sRGB mapping on scalar payloads")
    {
        storage::CreateOptions options;
        options.value_mapping = raster_store::pixel::Mapping::SRGBA;
        f.snapshot("srgb-left", { { key, tile(1.f, 1) } }, options);
        f.snapshot("right", { { key, tile(1.f, 1) } }, options);
        CHECK(thrown([&] { f.merge("srgb-left", "right", "[]", "merged"); }) == Error::Code::Unsupported);
        CHECK_FALSE(std::filesystem::exists(f.path("merged.part")));
    }
    SECTION("zoom gaps above 30 levels")
    {
        f.snapshot("right", { { { 32, { 0, 0 } }, tile(1.f, 1) } });
        CHECK(thrown([&] { f.merge("left", "right", "[]", "merged"); }) == Error::Code::Unsupported);
        CHECK_FALSE(std::filesystem::exists(f.path("merged.part")));
    }
    SECTION("payload type")
    {
        f.snapshot<glm::u8vec3>("right", { { key, tile(glm::u8vec3(1), 1) } });
        CHECK_THROWS_AS(f.merge("left", "right", "[]", "merged"), Error::Exception);
    }
    SECTION("physical tiles with physical descendants")
    {
        f.snapshot("right", { { key, tile(1.f, 1) }, { { 2, { 0, 0 } }, tile(1.f, 1) } });
        CHECK(thrown([&] { f.merge("left", "right", "[]", "merged"); }) == Error::Code::InvalidInput);
    }
    SECTION("incomplete input")
    {
        auto created = storage::create<float>(f.path("right"));
        REQUIRE(created);
        created->first.reset();
        auto options = f.options("left", "right.part", "[]", "merged");
        CHECK_THROWS_AS(merge::run(options), Error::Exception);
    }
    SECTION("existing destination")
    {
        f.snapshot("right", { { key, tile(1.f, 1) } });
        std::filesystem::create_directory(f.path("merged.part"));
        CHECK(thrown([&] { f.merge("left", "right", "[]", "merged"); }) == Error::Code::AlreadyExists);
    }
    SECTION("invalid priorities")
    {
        f.snapshot("right", { { key, tile(1.f, 1) } });
        CHECK(thrown([&] { f.merge("left", "right", "[1, 1]", "merged"); }) == Error::Code::InvalidInput);
        CHECK_FALSE(std::filesystem::exists(f.path("merged.part")));
    }
}

TEST_CASE("RF merger serial and parallel runs agree", "[rf-merger]")
{
    Fixture f;
    const Key coarse { 1, { 0, 0 } };
    f.snapshot("left", { { coarse, tile(5.f, 4) }, { { 1, { 1, 1 } }, tile(3.f, 0) } });
    f.snapshot("right", { { { 3, { 1, 2 } }, tile(9.f, 6) }, { { 4, { 0, 0 } }, tile(7.f, 8) }, { { 2, { 3, 3 } }, tile(2.f, 6) } });
    const auto serial = f.merge("left", "right", "[8, 4]", "serial");
    auto options = f.options("left", "right", "[8, 4]", "parallel");
    options.jobs = 4;
    const auto parallel = merge::run(options);
    CHECK(serial.statistics == parallel.statistics);
    auto a = f.open("serial");
    auto b = f.open("parallel");
    REQUIRE(physical(*a) == physical(*b));
    for (const auto& key : physical(*a)) {
        INFO(to_string(key));
        auto first = a->load(key);
        auto second = b->load(key);
        REQUIRE(first);
        REQUIRE(second);
        CHECK(same(first->data, second->data));
        CHECK(same(first->source_attribution, second->source_attribution));
    }
}

TEST_CASE("RF merger links unchanged tiles independently of output compression", "[rf-merger]")
{
    Fixture f;
    const Key key { 1, { 0, 0 } };
    f.snapshot("left", { { key, tile(1.f, 1) } });
    f.snapshot("right", { { { 2, { 3, 3 } }, tile(2.f, 2) }, { { 2, { 0, 0 } }, tile(2.f, 2) } });
    auto options = f.options("left", "right", "[1]", "merged");
    options.compression_algorithm = io::envelope::CompressionAlgorithm::None;
    options.checksum_algorithm = io::envelope::ChecksumAlgorithm::Crc32c;
    const auto report = merge::run(options);
    auto merged = f.open("merged");
    CHECK(linked(*f.open("right"), *merged, { 2, { 3, 3 } }));
    CHECK(report.statistics.left_only.tiles == 4);
    auto resampled = merged->load({ 2, { 1, 0 } });
    REQUIRE(resampled);
    CHECK(resampled->data.pixel({ 0, 0 }) == Catch::Approx(1.f));
}

TEST_CASE("RF merger failures retain a recoverable snapshot with statistics", "[rf-merger]")
{
    Fixture f;
    const Key coarse { 1, { 0, 0 } };
    const Key last { 2, { 3, 3 } };
    f.snapshot("left", { { coarse, tile(5.f, 4) }, { { 1, { 1, 1 } }, tile(3.f, 0) } });
    f.snapshot("right", { { { 4, { 1, 2 } }, tile(9.f, 6) }, { last, tile(2.f, 6) } });

    // The right tile is read by the last leaf in depth-first order.
    const auto payload = *f.open("right")->path_for(last);
    auto original = io::read_bytes_from_path(payload);
    REQUIRE(original);
    write_text(payload, "not a tile");
    CHECK(thrown([&] { f.merge("left", "right", "[6]", "failed"); }) == Error::Code::CorruptData);
    REQUIRE(io::write_bytes_to_path(*original, payload));
    const auto reference = f.merge("left", "right", "[6]", "reference");

    const auto part = f.path("failed.part");
    REQUIRE(std::filesystem::exists(part / "inputs.tmp"));
    REQUIRE(std::filesystem::exists(part / "statistics.tmp"));
    auto opened = storage::open<float>(part, { .allow_incomplete = true });
    REQUIRE(opened);
    const auto cached = physical(*opened->first);
    CHECK_FALSE(cached.empty());
    CHECK(cached.size() < reference.statistics.total().tiles);
    auto saved = statistics::read(part);
    REQUIRE(saved);
    CHECK(saved->total().tiles == cached.size());
    opened->first.reset();

    SECTION("recovery restores statistics and reuses cached tiles")
    {
        auto recovery = f.options("left", "right", "[6]", "recovered");
        recovery.cache = part;
        const auto recovered = merge::run(recovery);
        CHECK(recovered.statistics_complete);
        CHECK(recovered.restored_tiles == cached.size());
        CHECK(recovered.statistics == reference.statistics);
        auto cache = storage::open<float>(part, { .allow_incomplete = true });
        REQUIRE(cache);
        auto output = f.open("recovered");
        CHECK(physical(*output) == physical(*f.open("reference")));
        for (const auto& key : cached) {
            CHECK(linked(*cache->first, *output, key));
        }
    }
    SECTION("missing cache statistics are reported as incomplete")
    {
        std::filesystem::remove(part / "statistics.tmp");
        auto recovery = f.options("left", "right", "[6]", "recovered");
        recovery.cache = part;
        const auto recovered = merge::run(recovery);
        CHECK_FALSE(recovered.statistics_complete);
        CHECK(recovered.statistics.uncategorized.tiles == cached.size());
        CHECK(recovered.statistics.total().tiles == reference.statistics.total().tiles);
    }
    SECTION("changed priorities or input order invalidate the cache")
    {
        auto changed = f.options("left", "right", "[4, 6]", "recovered");
        changed.cache = part;
        CHECK(thrown([&] { merge::run(changed); }) == Error::Code::InvalidInput);
        auto swapped = f.options("right", "left", "[6]", "recovered2");
        swapped.cache = part;
        CHECK(thrown([&] { merge::run(swapped); }) == Error::Code::InvalidInput);
    }
    SECTION("published snapshots are not caches")
    {
        auto recovery = f.options("left", "right", "[6]", "recovered");
        recovery.cache = f.path("reference");
        CHECK(thrown([&] { merge::run(recovery); }) == Error::Code::InvalidInput);
    }
}

TEST_CASE("RF merger cancellation saves active tiles and stays recoverable", "[rf-merger]")
{
    Fixture f;
    f.snapshot("left", { { { 1, { 0, 0 } }, tile(5.f, 4) }, { { 1, { 1, 1 } }, tile(3.f, 0) } });
    f.snapshot("right", { { { 4, { 1, 2 } }, tile(9.f, 6) }, { { 2, { 3, 3 } }, tile(2.f, 6) } });
    const auto reference = f.merge("left", "right", "[6]", "reference");

    unsigned polls = 0;
    const auto options = f.options("left", "right", "[6]", "cancelled");
    CHECK(thrown([&] { merge::run(options, [&] { return ++polls > 4; }); }) == Error::Code::Cancelled);
    const auto part = f.path("cancelled.part");
    REQUIRE(std::filesystem::exists(part / "inputs.tmp"));
    auto opened = storage::open<float>(part, { .allow_incomplete = true });
    REQUIRE(opened);
    const auto cached = physical(*opened->first);
    CHECK_FALSE(cached.empty());
    CHECK(cached.size() < reference.statistics.total().tiles);
    auto saved = statistics::read(part);
    REQUIRE(saved);
    CHECK(saved->total().tiles == cached.size());
    opened->first.reset();

    auto recovery = f.options("left", "right", "[6]", "recovered");
    recovery.cache = part;
    const auto recovered = merge::run(recovery);
    CHECK(recovered.restored_tiles == cached.size());
    CHECK(recovered.statistics == reference.statistics);
}

TEST_CASE("RF merger empty inputs publish an empty snapshot", "[rf-merger]")
{
    Fixture f;
    f.snapshot("left", {});
    f.snapshot("right", {});
    const auto report = f.merge("left", "right", "[]", "merged");
    CHECK(report.statistics.total().tiles == 0);
    CHECK(physical(*f.open("merged")).empty());
}

TEST_CASE("RF merger statistics use one binary unit chosen from the total", "[rf-merger]")
{
    statistics::Totals totals;
    totals.add(statistics::Category::LinkedLeft, 3ull << 30);
    totals.add(statistics::Category::Mixed, 1ull << 30);
    auto lines = statistics::format(totals, true);
    REQUIRE(lines.size() == 7);
    CHECK(lines[1].find("3.00 GiB") != std::string::npos);
    CHECK(lines[1].find("( 75.0%)") != std::string::npos);
    CHECK(lines[3].find("1.00 GiB") != std::string::npos);
    CHECK(lines[6].find("4.00 GiB") != std::string::npos);

    statistics::Totals small;
    small.add(statistics::Category::RightOnly, 512 * 1024);
    CHECK(statistics::format(small, true)[5].find("0.50 MiB") != std::string::npos);

    statistics::Totals large;
    large.add(statistics::Category::LeftOnly, 2ull << 40);
    large.add(statistics::Category::Uncategorized, 1ull << 30);
    lines = statistics::format(large, false);
    REQUIRE(lines.size() == 8);
    CHECK(lines[0].find("incomplete") != std::string::npos);
    CHECK(lines[4].find("2.00 TiB") != std::string::npos);
    CHECK(lines[6].find("0.00 TiB") != std::string::npos);
}

TEST_CASE("RF merger command publishes and logs statistics", "[rf-merger]")
{
    Fixture f;
    f.snapshot("left", { { { 1, { 0, 0 } }, tile(1.f, 1) } });
    f.snapshot("right", { { { 2, { 0, 0 } }, tile(2.f, 2) }, { { 1, { 1, 1 } }, tile(3.f, 2) } });
    write_text(f.path("priority.json"), "[2]");
    const auto quote = [](const std::filesystem::path& path) { return "'" + path.string() + "'"; };
    const auto log = f.path("command.log");
    const auto command = [&](const std::string& arguments) {
        return std::system((quote(ALP_RF_MERGER_PATH) + " --left " + quote(f.path("left")) + " --right " + quote(f.path("right")) + " --priorities "
            + quote(f.path("priority.json")) + arguments + " > " + quote(log) + " 2>&1")
                .c_str());
    };
    REQUIRE(command(" --output " + quote(f.path("merged")) + " --jobs 2 --compression zstd-best") == 0);
    std::ifstream stream(f.path("merged.log"));
    const std::string text { std::istreambuf_iterator<char>(stream), {} };
    CHECK(text.find("Published " + f.path("merged").string() + ": 5 tiles, 64x64 pixels per tile") != std::string::npos);
    CHECK(text.find("RF merge statistics:") != std::string::npos);
    CHECK(text.find("hard-linked from right (shared with input)") != std::string::npos);
    CHECK(text.find("5/5 leaves (100.0%)") != std::string::npos);
    CHECK(f.open("merged"));
    CHECK(command(" --output " + quote(f.path("bad")) + " --compression lz4") != 0);
    CHECK(command(" --output " + quote(f.path("merged"))) != 0);
}

#include "../temporary_directory.h"
#include "HttpFixture.h"
#include "raster_store/storage.h"
#include "tiles/TileWorker.h"
#include "tiles/build.h"
#include "tiles/jpeg.h"
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <fstream>
#include <map>
#include <opencv2/imgcodecs.hpp>
#include <set>
#include <spdlog/sinks/ostream_sink.h>
#include <sstream>

namespace {
namespace tiles = rf_builder::tiles;
namespace run = rf_builder::run;
namespace storage = raster_store::storage;
using Key = run::Key;
using Bytes = std::vector<std::uint8_t>;
void write_text(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream stream(path);
    stream << text;
    REQUIRE(stream.good());
}
std::string path(const Key& key) { return fmt::format("/{}/{}/{}.jpeg", key.zoom_level, key.coords.x, key.coords.y); }
Bytes image(unsigned side, cv::Scalar colour = { 0, 0, 0 }, bool pattern = false)
{
    cv::Mat pixels(int(side), int(side), CV_8UC3, colour);
    if (pattern) {
        for (unsigned y = 0; y < side; ++y) {
            for (unsigned x = 0; x < side; ++x) {
                pixels.at<cv::Vec3b>(int(y), int(x)) = { std::uint8_t(x * 17), std::uint8_t(y * 23), 200 };
            }
        }
    }
    Bytes result;
    REQUIRE(cv::imencode(".jpg", pixels, result));
    return result;
}
std::string json(const std::string& base, unsigned minimum = 3, unsigned maximum = 5, unsigned side = 8)
{
    return fmt::format(
        R"({{"url_pattern":"{}/{{zoom}}/{{x}}/{{y}}.jpeg","y_direction":"down","min_zoom":{},"max_zoom":{},"tile_size":{}}})", base, minimum, maximum, side);
}
void mask(const std::filesystem::path& file, const RasterTransform::Bounds& bounds)
{
    write_text(file,
        fmt::format(
            R"({{"type":"FeatureCollection","crs":{{"type":"name","properties":{{"name":"EPSG:3857"}}}},"features":[{{"type":"Feature","properties":{{}},"geometry":{{"type":"Polygon","coordinates":[[[{0},{1}],[{2},{1}],[{2},{3}],[{0},{3}],[{0},{1}]]]}}}}]}})",
            bounds.min.x,
            bounds.min.y,
            bounds.max.x,
            bounds.max.y));
}
struct Fixture {
    test::TemporaryDirectory directory { "rf-online" };
    unsigned transient_failures = 0;
    std::map<std::string, Bytes> pyramid;
    rf_test::HttpFixture server { [this](const std::string& request, unsigned attempt) {
        if (attempt <= transient_failures) {
            return rf_test::Response { 503, {}, "" };
        }
        auto found = pyramid.find(request);
        return found == pyramid.end() ? rf_test::Response() : rf_test::Response { 200, found->second, "Content-Type: image/jpeg\r\n" };
    } };
    tiles::Options options;
    Key root { 2, { 1, 1 } };
    Fixture()
    {
        options.provider = directory.path() / "provider.json";
        options.mask = (directory.path() / "mask.geojson").string();
        options.output = { directory.path() / "result", 16, 1, std::nullopt, 1 };
        options.retry = { std::chrono::milliseconds(5), std::chrono::milliseconds(100), std::chrono::milliseconds(30), std::chrono::milliseconds(20) };
        write_text(options.provider, json(server.base()));
        mask(options.mask, RasterTransform::tile_bounds(root));
        const std::string entity
            = R"({"spatial_resolution":1,"acquisition_date":"2026","ingestion_date":"today","copyright":"test","copyright_link":"https://example.org","license":"test"})";
        write_text(directory.path() / "source_attribution_table.json", "[" + entity + "," + entity + "]");
        for (unsigned y = 2; y < 4; ++y) {
            for (unsigned x = 2; x < 4; ++x) {
                pyramid[path({ 3, { x, y } })] = image(8, { 50, 100, 150 });
            }
        }
    }
    void mixed()
    {
        pyramid[path({ 4, { 4, 4 } })] = image(8, {}, true);
        pyramid[path({ 5, { 8, 8 } })] = image(8); // Valid black finest patch.
        pyramid[path({ 5, { 10, 10 } })] = image(8, { 255, 255, 255 }); // Unreachable below 404 at 4/5/5.
    }
    tiles::inputs::Record record() const
    {
        tiles::inputs::Record result;
        result.provider = *tiles::provider::read(options.provider);
        result.tile_side = options.output.tile_side;
        result.mask = *run::identifier(options.mask);
        result.attribution_index = 1;
        result.attribution = *run::attribution(options.output);
        result.decoder_version = tiles::jpeg::version();
        return result;
    }
};
std::set<Key> keys(const auto& snapshot)
{
    std::set<Key> result;
    for (const auto& [key, status] : snapshot.index()) {
        if (status != store::NodeStatus::Virtual) {
            CHECK(status == store::NodeStatus::Leaf);
            result.insert(key);
        }
    }
    return result;
}
} // namespace

TEST_CASE("Online provider validates strict JSON types dimensions URLs and zoom arithmetic", "[rf-builder][online]")
{
    const auto good = json("https://example.org");
    REQUIRE(tiles::provider::parse(good));
    for (const auto bad : { "{}", "[]", "null", "{", "/*comment*/" }) {
        CHECK_FALSE(tiles::provider::parse(bad));
    }
    CHECK_FALSE(tiles::provider::parse(good + "garbage"));
    const auto replace = [&](const std::string& before, const std::string& after) {
        auto text = good;
        const auto at = text.find(before);
        REQUIRE(at != std::string::npos);
        text.replace(at, before.size(), after);
        return text;
    };
    for (const auto value : { "-1", "3.0", "true", "null", "\"3\"", "4294967296", "33" }) {
        CHECK_FALSE(tiles::provider::parse(replace("\"min_zoom\":3", "\"min_zoom\":" + std::string(value))));
    }
    CHECK_FALSE(tiles::provider::parse(replace("\"tile_size\":8", "\"tile_size\":7")));
    CHECK_FALSE(tiles::provider::parse(replace("\"tile_size\":8", "\"tile_size\":65536")));
    CHECK_FALSE(tiles::provider::parse(replace("\"tile_size\":8", "\"extra\":8")));
    CHECK_FALSE(tiles::provider::parse(replace("\"down\"", "\"other\"")));
    CHECK_FALSE(tiles::provider::parse(replace("https://example.org", "file:///tmp")));
    CHECK_FALSE(tiles::provider::parse(replace("{zoom}", "{z}")));
    auto settings = *tiles::provider::parse(good);
    CHECK(tiles::provider::zoom_offset(settings, 16) == 1);
    CHECK_FALSE(tiles::provider::zoom_offset(settings, 3));
    CHECK_FALSE(tiles::provider::zoom_offset(settings, 24));
    CHECK_FALSE(tiles::provider::zoom_offset(settings, 256));
    settings.y_direction = tiles::provider::YDirection::Up;
    CHECK(tiles::provider::url(settings, { 32, { 4294967295u, 0 } }) == "https://example.org/32/4294967295/4294967295.jpeg");
}

TEST_CASE("Online JPEG decoding preserves orientation channels and native values", "[rf-builder][online]")
{
    const auto encoded = image(8, {}, true);
    auto decoded = tiles::jpeg::decode(encoded, 8);
    REQUIRE(decoded);
    auto reference = cv::imdecode(encoded, cv::IMREAD_COLOR | cv::IMREAD_IGNORE_ORIENTATION);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const auto expected = reference.at<cv::Vec3b>(y, x);
            CHECK(decoded->at<cv::Vec3b>(y, x) == cv::Vec3b(expected[2], expected[1], expected[0]));
        }
    }
    // APP1 Exif orientation 6 (rotate 90 degrees); geospatial rows must stay put.
    const Bytes exif { 0xff, 0xe1, 0, 34, 'E', 'x', 'i', 'f', 0, 0, 'I', 'I', 42, 0, 8, 0, 0, 0, 1, 0, 0x12, 1, 3, 0, 1, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0 };
    auto oriented = encoded;
    oriented.insert(oriented.begin() + 2, exif.begin(), exif.end());
    auto unchanged = tiles::jpeg::decode(oriented, 8);
    REQUIRE(unchanged);
    CHECK(cv::norm(*decoded, *unchanged, cv::NORM_INF) == 0);
    CHECK_FALSE(tiles::jpeg::decode(encoded, 16));
    CHECK_FALSE(tiles::jpeg::decode(Bytes { 'n', 'o' }, 8));
    auto truncated = encoded;
    truncated.resize(truncated.size() / 2);
    CHECK_FALSE(tiles::jpeg::decode(truncated, 8));
    Bytes png;
    REQUIRE(cv::imencode(".png", reference, png));
    CHECK_FALSE(tiles::jpeg::decode(png, 8));
    CHECK(tiles::jpeg::nonlinear(0.5) == 188);
    for (unsigned i = 0; i < 256; ++i) {
        CHECK(tiles::jpeg::nonlinear(tiles::jpeg::linear(std::uint8_t(i))) == i);
    }
    CHECK(tiles::jpeg::version().find("libjpeg-turbo") != std::string::npos);
}

TEST_CASE("Online HTTP retries transient failures and only 404 means absence", "[rf-builder][online]")
{
    using namespace std::chrono_literals;
    rf_test::HttpFixture server([](const std::string& request, unsigned attempt) {
        if (request == "/redirect") {
            return rf_test::Response { 302, { 'r', 'e', 'd' }, "Location: /retry\r\n" };
        }
        if (request == "/retry") {
            return rf_test::Response { attempt < 3 ? 503 : 200, { 'o', 'k' }, "" };
        }
        if (request == "/unauthorized") {
            return rf_test::Response { 401, {}, "" };
        }
        if (request == "/throttle") {
            return rf_test::Response { 429, {}, "Retry-After: 3600\r\n" };
        }
        if (request == "/slow") {
            return rf_test::Response { 200, { 'o', 'k' }, "", 40ms };
        }
        if (request == "/large") {
            return rf_test::Response { 200, Bytes(2000), "" };
        }
        return rf_test::Response();
    });
    tiles::NetworkCounters counters;
    tiles::HttpClient client(counters, 1024, { 5ms, 80ms, 15ms, 15ms });
    auto retried = client.get(server.base() + "/retry");
    REQUIRE(retried);
    REQUIRE(*retried);
    CHECK(**retried == Bytes { 'o', 'k' });
    CHECK(counters.requests == 3);
    auto redirected = client.get(server.base() + "/redirect");
    REQUIRE(redirected);
    REQUIRE(*redirected);
    CHECK(**redirected == Bytes { 'o', 'k' });
    auto missing = client.get(server.base() + "/missing");
    REQUIRE(missing);
    CHECK_FALSE(*missing);
    const auto before = counters.requests.load();
    CHECK_FALSE(client.get(server.base() + "/unauthorized"));
    CHECK(counters.requests == before + 1);
    const auto started = std::chrono::steady_clock::now();
    CHECK_FALSE(client.get(server.base() + "/throttle"));
    CHECK(std::chrono::steady_clock::now() - started < 1s);
    CHECK_FALSE(client.get(server.base() + "/slow"));
    CHECK_FALSE(client.get(server.base() + "/large"));
}

TEST_CASE("Online adaptive RF keeps fine islands and fills coarse siblings without overlap", "[rf-builder][online]")
{
    Fixture fixture;
    fixture.mixed();
    fixture.options.output.jobs = GENERATE(1u, 4u);
    auto report = tiles::build(fixture.options);
    INFO((report ? "success" : report.error().to_string()));
    REQUIRE(report);
    CHECK(report->tile_count == 7);
    auto output = storage::open<glm::u8vec3>(fixture.options.output.output);
    REQUIRE(output);
    const auto physical = keys(*output);
    CHECK(physical.size() == 7);
    CHECK_FALSE(physical.contains(fixture.root));
    for (const auto& key : physical) {
        auto parent = key;
        while (parent.zoom_level) {
            parent = parent.parent();
            CHECK_FALSE(physical.contains(parent));
        }
    }
    auto fine = output->load({ 4, { 4, 4 } });
    REQUIRE(fine);
    for (unsigned y = 0; y < 8; ++y) {
        for (unsigned x = 0; x < 8; ++x) {
            CHECK(fine->data.buffer()[y * 16 + x] == glm::u8vec3(0));
            CHECK(fine->source_attribution.buffer()[y * 16 + x] == 1);
        }
    }
    const auto requests = fixture.server.requests();
    CHECK(std::ranges::find(requests, path({ 5, { 10, 10 } })) == requests.end());
    CHECK(std::ranges::none_of(requests, [](const auto& request) { return request.starts_with("/6/") || request.starts_with("/2/"); }));
}

TEST_CASE("Online RF native assembled pixels equal decoded JPEGs", "[rf-builder][online]")
{
    Fixture fixture;
    write_text(fixture.options.provider, json(fixture.server.base(), 3, 3));
    fixture.pyramid[path({ 3, { 2, 2 } })] = image(8, {}, true);
    auto report = tiles::build(fixture.options);
    REQUIRE(report);
    CHECK(report->tile_count == 1);
    auto output = storage::open<glm::u8vec3>(fixture.options.output.output);
    REQUIRE(output);
    auto tile = output->load(fixture.root);
    REQUIRE(tile);
    const auto expected = tiles::jpeg::decode(fixture.pyramid.at(path({ 3, { 2, 2 } })), 8);
    REQUIRE(expected);
    for (unsigned y = 0; y < 8; ++y) {
        for (unsigned x = 0; x < 8; ++x) {
            const auto value = expected->at<cv::Vec3b>(int(y), int(x));
            CHECK(tile->data.buffer()[y * 16 + x] == glm::u8vec3(value[0], value[1], value[2]));
        }
    }
}

TEST_CASE("Online fallback samples across RF and source boundaries in linear light", "[rf-builder][online]")
{
    Fixture fixture;
    const Key candidate { 4, { 5, 4 } };
    mask(fixture.options.mask, RasterTransform::tile_bounds(candidate));
    fixture.pyramid.clear();
    fixture.pyramid[path({ 3, { 2, 2 } })] = image(8);
    fixture.pyramid[path({ 3, { 3, 2 } })] = image(8, { 255, 255, 255 }); // Outside mask, still contributes.
    auto record = fixture.record();
    auto selected = rf_builder::Mask::open(record.mask);
    REQUIRE(selected);
    tiles::planning::Coverage coverage(selected->bounds());
    tiles::NetworkCounters counters;
    auto worker = tiles::TileWorker::open(record, coverage, counters, 4096, fixture.options.retry);
    REQUIRE(worker);
    auto result = (*worker)->prepare(candidate);
    REQUIRE(result);
    auto* tile = std::get_if<raster_store::Tile<glm::u8vec3>>(&*result);
    REQUIRE(tile);
    const auto value = tile->data.buffer()[8 * 16 + 15];
    CHECK(value == glm::u8vec3(tiles::jpeg::nonlinear(0.375)));
    CHECK(value.x != 96); // Nonlinear interpolation would be too dark.
    CHECK((*worker)->retained_bytes() <= 4096);
    const auto requests = fixture.server.requests();
    CHECK(std::ranges::find(requests, path({ 3, { 3, 2 } })) != requests.end());
}

TEST_CASE("Online conservative coverage weights preserve area across overlaps and subdivision", "[rf-builder][online]")
{
    const Key root { 2, { 1, 1 } };
    const auto bounds = RasterTransform::tile_bounds(root);
    tiles::planning::Coverage coverage({ bounds, bounds, RasterTransform::tile_bounds({ 3, { 2, 2 } }) });
    CHECK(coverage.weight(root) == Catch::Approx(1. / 16));
    double total = 0;
    for (const auto& child : coverage.children(root).children) {
        total += coverage.weight(child);
    }
    CHECK(total == Catch::Approx(coverage.weight(root)));
    tiles::planning::Cursor cursor(coverage, 3);
    unsigned count = 0;
    for (;;) {
        auto next = cursor.next();
        REQUIRE(next);
        if (!*next) {
            break;
        }
        ++count;
    }
    CHECK(count == 4);
}

TEST_CASE("Online minimum missing coverage publishes empty and malformed sources abort", "[rf-builder][online]")
{
    Fixture fixture;
    const bool malformed = GENERATE(false, true);
    fixture.pyramid.clear();
    if (malformed) {
        fixture.pyramid[path({ 3, { 2, 2 } })] = { 'b', 'a', 'd' };
    }
    auto result = tiles::build(fixture.options);
    if (malformed) {
        CHECK_FALSE(result);
        CHECK_FALSE(std::filesystem::exists(fixture.options.output.output));
    } else {
        REQUIRE(result);
        CHECK(result->tile_count == 0);
        const auto requests = fixture.server.requests();
        CHECK(std::ranges::all_of(requests, [](const auto& request) { return request.starts_with("/3/"); }));
    }
}

TEST_CASE("Online completed cache skips all HTTP and provider identity uses field values", "[rf-builder][online]")
{
    Fixture fixture;
    fixture.mixed();
    auto partial = fixture.options.output.output;
    partial += ".part";
    auto cancelled = tiles::build(fixture.options, [&] {
        unsigned count = 0;
        if (std::filesystem::exists(partial)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(partial)) {
                if (entry.path().extension() == ".amort") {
                    ++count;
                }
            }
        }
        return count == 7;
    });
    REQUIRE_FALSE(cancelled);
    CHECK(cancelled.error().code() == Error::Code::Cancelled);
    REQUIRE(std::filesystem::exists(partial / "inputs.tmp"));
    fixture.options.output.cache = partial;
    fixture.options.output.output = fixture.directory.path() / "resumed";
    fixture.options.provider = fixture.directory.path() / "moved.json";
    write_text(fixture.options.provider, "\n " + json(fixture.server.base()) + " \n");
    fixture.server.clear();
    auto resumed = tiles::build(fixture.options);
    REQUIRE(resumed);
    CHECK(resumed->reused_tiles == 7);
    CHECK(fixture.server.requests().empty());
    auto output = storage::open<glm::u8vec3>(fixture.options.output.output);
    REQUIRE(output);
    for (const auto& key : keys(*output)) {
        const auto path = *output->path_for(key);
        CHECK(std::filesystem::equivalent(path, partial / path.lexically_relative(fixture.options.output.output)));
    }
    fixture.options.output.output = fixture.directory.path() / "changed";
    write_text(fixture.options.provider, json(fixture.server.base(), 3, 4));
    CHECK_FALSE(tiles::build(fixture.options));
    CHECK(fixture.server.requests().empty());
    CHECK_FALSE(std::filesystem::exists(fixture.options.output.output.string() + ".part"));
}

TEST_CASE("Online fallback wraps longitude extends true coverage edges and never wraps latitude", "[rf-builder][online]")
{
    Fixture fixture;
    const unsigned scenario = GENERATE(0u, 1u, 2u);
    const Key candidate = scenario == 2 ? Key { 4, { 4, 0 } } : Key { 4, { 15, 4 } };
    mask(fixture.options.mask, RasterTransform::tile_bounds(candidate));
    fixture.pyramid.clear();
    const Key supplying = scenario == 2 ? Key { 3, { 2, 0 } } : Key { 3, { 7, 2 } };
    fixture.pyramid[path(supplying)] = image(8);
    if (scenario == 0) {
        fixture.pyramid[path({ 3, { 0, 2 } })] = image(8, { 255, 255, 255 });
    }
    if (scenario == 2) {
        fixture.pyramid[path({ 3, { 2, 7 } })] = image(8, { 255, 255, 255 });
    }
    const auto record = fixture.record();
    auto mask_data = rf_builder::Mask::open(record.mask);
    REQUIRE(mask_data);
    const tiles::planning::Coverage coverage(mask_data->bounds());
    tiles::NetworkCounters counters;
    auto worker = tiles::TileWorker::open(record, coverage, counters, 256, fixture.options.retry);
    REQUIRE(worker);
    auto result = (*worker)->prepare(candidate);
    REQUIRE(result);
    const auto* tile = std::get_if<raster_store::Tile<glm::u8vec3>>(&*result);
    REQUIRE(tile);
    CHECK((*worker)->retained_bytes() <= 256); // Every decoded image exceeds this budget and is evicted/not retained.
    if (scenario == 0) {
        CHECK(tile->data.buffer()[8 * 16 + 15] == glm::u8vec3(tiles::jpeg::nonlinear(0.375)));
    } else {
        CHECK(std::ranges::all_of(tile->data.buffer(), [](const auto& pixel) { return pixel == glm::u8vec3(0); }));
    }
    if (scenario == 2) {
        const auto requests = fixture.server.requests();
        CHECK(std::ranges::find(requests, path({ 3, { 2, 7 } })) == requests.end());
    }
}

TEST_CASE("Online fallback agrees when the same source region is split into RF windows", "[rf-builder][online]")
{
    Fixture fixture;
    fixture.pyramid[path({ 3, { 2, 2 } })] = image(8, {}, true);
    const auto whole = Key { 3, { 2, 2 } };
    mask(fixture.options.mask, RasterTransform::tile_bounds(whole));
    auto record = fixture.record();
    auto mask_data = rf_builder::Mask::open(record.mask);
    REQUIRE(mask_data);
    const tiles::planning::Coverage coverage(mask_data->bounds());
    tiles::NetworkCounters counters;
    auto worker = tiles::TileWorker::open(record, coverage, counters, 4096, fixture.options.retry);
    REQUIRE(worker);
    std::array<raster_store::Tile<glm::u8vec3>, 4> parts {
        raster_store::Tile<glm::u8vec3>(16), raster_store::Tile<glm::u8vec3>(16), raster_store::Tile<glm::u8vec3>(16), raster_store::Tile<glm::u8vec3>(16)
    };
    const auto children = whole.children();
    for (unsigned i = 0; i < 4; ++i) {
        auto result = (*worker)->prepare(children[i]);
        REQUIRE(result);
        auto* tile = std::get_if<raster_store::Tile<glm::u8vec3>>(&*result);
        REQUIRE(tile);
        parts[i] = std::move(*tile);
    }
    record.tile_side = 32; // Same output pixel spacing; one window covers all four parts.
    auto metatile_worker = tiles::TileWorker::open(record, coverage, counters, 4096, fixture.options.retry);
    REQUIRE(metatile_worker);
    auto metatile = (*metatile_worker)->prepare(whole);
    REQUIRE(metatile);
    const auto* joined = std::get_if<raster_store::Tile<glm::u8vec3>>(&*metatile);
    REQUIRE(joined);
    for (unsigned i = 0; i < 4; ++i) {
        for (unsigned y = 0; y < 16; ++y) {
            for (unsigned x = 0; x < 16; ++x) {
                CHECK(parts[i].data.buffer()[y * 16 + x] == joined->data.buffer()[(y + (i / 2) * 16) * 32 + x + (i % 2) * 16]);
            }
        }
    }
}

TEST_CASE("Online narrow masks survive conservative refinement and holes select output centres", "[rf-builder][online]")
{
    Fixture fixture;
    fixture.mixed();
    auto bounds = RasterTransform::tile_bounds({ 5, { 8, 8 } });
    const auto spacing = bounds.width() / 8;
    // This interval misses all centres at source zoom 3 but contains zoom-5 centres.
    bounds.min.x += spacing * 0.4;
    bounds.max.x = bounds.min.x + spacing * 0.2;
    mask(fixture.options.mask, bounds);
    auto result = tiles::build(fixture.options);
    REQUIRE(result);
    CHECK(result->tile_count > 0);
    auto output = storage::open<glm::u8vec3>(fixture.options.output.output);
    REQUIRE(output);
    for (const auto& key : keys(*output)) {
        CHECK(key.zoom_level == 4);
    }
    fixture.options.output.output = fixture.directory.path() / "hole";
    const auto outer = RasterTransform::tile_bounds(fixture.root);
    const auto hole = RasterTransform::tile_bounds({ 4, { 5, 5 } });
    write_text(fixture.options.mask,
        fmt::format(
            R"({{"type":"FeatureCollection","crs":{{"type":"name","properties":{{"name":"EPSG:3857"}}}},"features":[{{"type":"Feature","properties":{{}},"geometry":{{"type":"Polygon","coordinates":[[[{0},{1}],[{2},{1}],[{2},{3}],[{0},{3}],[{0},{1}]],[[{4},{5}],[{4},{7}],[{6},{7}],[{6},{5}],[{4},{5}]]]}}}}]}})",
            outer.min.x,
            outer.min.y,
            outer.max.x,
            outer.max.y,
            hole.min.x,
            hole.min.y,
            hole.max.x,
            hole.max.y));
    auto holed = tiles::build(fixture.options);
    INFO((holed ? "success" : holed.error().to_string()));
    REQUIRE(holed);
    auto hole_output = storage::open<glm::u8vec3>(fixture.options.output.output);
    REQUIRE(hole_output);
    CHECK_FALSE(keys(*hole_output).contains(Key { 4, { 5, 5 } }));
}

TEST_CASE("Online partial cache preserves completed leaves while discovering unfinished siblings", "[rf-builder][online]")
{
    Fixture fixture;
    fixture.mixed();
    auto partial = fixture.options.output.output;
    partial += ".part";
    auto cancelled = tiles::build(fixture.options, [&] { return std::filesystem::exists(partial / "4/4/4.amort"); });
    REQUIRE_FALSE(cancelled);
    CHECK(cancelled.error().code() == Error::Code::Cancelled);
    auto cached = storage::open<glm::u8vec3>(partial, { .allow_incomplete = true });
    REQUIRE(cached);
    const auto completed = keys(*cached);
    REQUIRE_FALSE(completed.empty());
    REQUIRE(completed.size() < 7);
    // An unindexed file is not a completed region.
    std::filesystem::create_directories(partial / "3/3");
    write_text(partial / "3/3/3.amort", "unindexed garbage");
    fixture.options.output.cache = partial;
    fixture.options.output.output = fixture.directory.path() / "resumed";
    fixture.options.output.jobs = 4;
    fixture.server.clear();
    auto resumed = tiles::build(fixture.options);
    REQUIRE(resumed);
    CHECK(resumed->tile_count == 7);
    CHECK(resumed->reused_tiles == completed.size());
    auto output = storage::open<glm::u8vec3>(fixture.options.output.output);
    REQUIRE(output);
    const auto physical = keys(*output);
    for (const auto& key : completed) {
        CHECK(std::filesystem::equivalent(*output->path_for(key), *cached->path_for(key)));
    }
    for (const auto& key : physical) {
        auto ancestor = key;
        while (ancestor.zoom_level) {
            ancestor = ancestor.parent();
            CHECK_FALSE(physical.contains(ancestor));
        }
    }
    CHECK_FALSE(fixture.server.requests().empty());
}

TEST_CASE("Online CLI requires the subcommand and provider file and documents JSON", "[rf-builder][online][cli]")
{
    Fixture fixture;
    const auto quote = [](const std::string& text) {
        std::string result = "'";
        for (char c : text) {
            result += c == '\'' ? "'\\''" : std::string(1, c);
        }
        return result + "'";
    };
    const auto log = fixture.directory.path() / "cli.log";
    const auto command
        = [&](const std::string& arguments) { return std::system((quote(ALP_RF_BUILDER_PATH) + arguments + " > " + quote(log.string()) + " 2>&1").c_str()); };
    REQUIRE(command(" tiles --help") == 0);
    std::ifstream stream(log);
    const std::string help { std::istreambuf_iterator<char>(stream), {} };
    CHECK(help.find("--provider providers/basemap.json") != std::string::npos);
    CHECK(help.find("url_pattern") != std::string::npos);
    CHECK(help.find("ceiling") != std::string::npos);
    CHECK(command(" --dataset absent") != 0);
    CHECK(command(" tiles --url https://example.org") != 0);
    CHECK(command(" tiles --provider absent") != 0);
    fixture.server.configure([&] { fixture.transient_failures = 1; });
    write_text(fixture.options.provider, json(fixture.server.base(), 3, 3));
    const auto args = " tiles --provider " + quote(fixture.options.provider.string()) + " --mask " + quote(fixture.options.mask) + " --output "
        + quote(fixture.options.output.output.string()) + " --attribution-index 1 --tile-size 16 --jobs 2";
    REQUIRE(command(args) == 0);
    std::ifstream persisted(fixture.options.output.output.string() + ".log");
    const std::string output { std::istreambuf_iterator<char>(persisted), {} };
    CHECK(output.find("estimated 100.0%") != std::string::npos);
    CHECK(output.find("downloaded bytes") != std::string::npos);
    CHECK(output.find("Published") != std::string::npos);
    CHECK(output.find("retry wait 500 ms") != std::string::npos);
    CHECK(output.find("remaining budget") != std::string::npos);
}

TEST_CASE("Online source settings and cache failures abort before requests", "[rf-builder][online]")
{
    Fixture fixture;
    SECTION("missing provider") { fixture.options.provider = fixture.directory.path() / "absent"; }
    SECTION("wrong source ratio") { fixture.options.output.tile_side = 24; }
    SECTION("negative start zoom") { fixture.options.output.tile_side = 256; }
    SECTION("missing cache record") { fixture.options.output.cache = fixture.directory.path() / "missing.part"; }
    SECTION("corrupt cache record")
    {
        fixture.options.output.cache = fixture.directory.path() / "corrupt.part";
        std::filesystem::create_directory(*fixture.options.output.cache);
        write_text(*fixture.options.output.cache / "inputs.tmp", "broken");
    }
    SECTION("source settings mismatch")
    {
        fixture.options.output.cache = fixture.directory.path() / "changed.part";
        std::filesystem::create_directory(*fixture.options.output.cache);
        auto record = fixture.record();
        record.provider.tile_size = 16;
        REQUIRE(io::envelope::write_to_path<tiles::inputs::Schema>(record, *fixture.options.output.cache / "inputs.tmp"));
    }
    CHECK_FALSE(tiles::build(fixture.options));
    CHECK(fixture.server.requests().empty());
    CHECK_FALSE(std::filesystem::exists(fixture.options.output.output.string() + ".part"));
}

TEST_CASE("Online coordinator ignores active subdivision after cancellation", "[rf-builder][online][parallel]")
{
    Fixture fixture;
    run::Source<glm::u8vec3> source;
    source.attribution = *run::attribution(fixture.options.output);
    source.write_inputs = [](const auto& path) -> Expected<void> {
        write_text(path, "test inputs");
        return {};
    };
    source.total = [](const auto&) -> Expected<double> { return 1.; };
    bool scheduled = false;
    source.next = [&](const auto&) -> Expected<std::optional<Key>> {
        if (scheduled) {
            return std::nullopt;
        }
        scheduled = true;
        return std::optional(Key { 0, { 0, 0 } });
    };
    source.initialize = [](unsigned, const auto&) -> Expected<void> { return {}; };
    std::atomic<bool> active = false;
    std::atomic<bool> stop_seen = false;
    std::atomic<unsigned> prepared = 0;
    source.prepare = [&](unsigned, const Key& key) -> Expected<run::Prepared<glm::u8vec3>> {
        ++prepared;
        active = true;
        while (!stop_seen) {
            std::this_thread::yield();
        }
        const auto children = key.children();
        return run::Prepared<glm::u8vec3>(run::Subdivide { { children.begin(), children.end() } });
    };
    source.weight = [](const Key& key) { return std::ldexp(1., -2 * int(key.zoom_level)); };
    auto result = run::execute(fixture.options.output, std::move(source), [&] {
        if (active) {
            stop_seen = true;
            return true;
        }
        return false;
    });
    REQUIRE_FALSE(result);
    CHECK(result.error().code() == Error::Code::Cancelled);
    CHECK(prepared == 1);
    auto output = storage::open<glm::u8vec3>(fixture.options.output.output.string() + ".part", { .allow_incomplete = true });
    REQUIRE(output);
    CHECK(keys(*output).empty());
}

TEST_CASE("Online coordinator reports weighted progress during idle and out-of-order work", "[rf-builder][online][parallel]")
{
    Fixture fixture;
    fixture.options.output.jobs = 2;
    run::Source<glm::u8vec3> source;
    source.attribution = *run::attribution(fixture.options.output);
    source.write_inputs = [](const auto& path) -> Expected<void> {
        write_text(path, "test inputs");
        return {};
    };
    source.total = [](const auto&) -> Expected<double> { return 1.; };
    bool scheduled = false;
    source.next = [&](const auto&) -> Expected<std::optional<Key>> {
        if (scheduled) {
            return std::nullopt;
        }
        scheduled = true;
        return std::optional(Key { 0, { 0, 0 } });
    };
    source.initialize = [](unsigned, const auto&) -> Expected<void> { return {}; };
    source.refine_cached = [](const Key& key) {
        const auto children = key.children();
        return run::Subdivide { { children.begin(), children.end() } };
    };
    source.network_stats = [] { return run::NetworkStats(); };
    source.weight = [](const Key& key) { return std::ldexp(1., -2 * int(key.zoom_level)); };
    source.prepare = [](unsigned, const Key& key) -> Expected<run::Prepared<glm::u8vec3>> {
        if (key.zoom_level == 0) {
            const auto children = key.children();
            return run::Prepared<glm::u8vec3>(run::Subdivide { { children.begin(), children.end() } });
        }
        if (key.coords == glm::uvec2(0, 0)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10300));
        }
        if (key.coords == glm::uvec2(1, 1)) {
            return run::Prepared<glm::u8vec3>(std::monostate());
        }
        raster_store::Tile<glm::u8vec3> tile(16);
        std::ranges::fill(tile.source_attribution.buffer(), 1);
        return run::Prepared<glm::u8vec3>(std::move(tile));
    };
    std::ostringstream stream;
    const auto logger = Log::get_logger();
    const auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
    logger->sinks().push_back(sink);
    auto result = run::execute(fixture.options.output, std::move(source));
    logger->sinks().pop_back();
    REQUIRE(result);
    CHECK(result->tile_count == 3);
    const auto log = stream.str();
    CHECK(log.find("estimated 25.0%") != std::string::npos);
    CHECK(log.find("estimated 75.0%") != std::string::npos); // Periodic update while the first child is still active.
    CHECK(log.find("estimated 100.0%") != std::string::npos);
}

TEST_CASE("Online mixed-resolution import measurement", "[.][online-benchmark]")
{
    Fixture fixture;
    const tiles::planning::Coverage weights({ RasterTransform::tile_bounds({ 0, { 0, 0 } }) });
    unsigned weight_sample = 0;
    BENCHMARK("Online geographic completion weight")
    {
        const unsigned coordinate = weight_sample++ % 1024;
        return weights.weight({ 10, { coordinate, coordinate } });
    };
    const unsigned side = 256, output_side = 4096;
    fixture.options.output.tile_side = output_side;
    fixture.options.output.jobs = 2;
    fixture.options.retry = {};
    fixture.root = { 0, { 0, 0 } };
    mask(fixture.options.mask, RasterTransform::tile_bounds(fixture.root));
    write_text(fixture.options.provider, json(fixture.server.base(), 4, 6, side));
    fixture.pyramid.clear();
    const auto coarse = image(side, { 50, 100, 150 });
    for (unsigned y = 0; y < 16; ++y) {
        for (unsigned x = 0; x < 16; ++x) {
            fixture.pyramid[path({ 4, { x, y } })] = coarse;
        }
    }
    fixture.pyramid[path({ 5, { 16, 16 } })] = image(side, { 75, 125, 175 });
    fixture.pyramid[path({ 6, { 32, 32 } })] = image(side, { 100, 150, 200 });
    const auto started = std::chrono::steady_clock::now();
    auto result = tiles::build(fixture.options);
    REQUIRE(result);
    CHECK(result->tile_count == 7);
    LOG_INFO("Online mixed-resolution measurement: {:.3f}s, {} output tiles, {} stored bytes, {} requests; dense pixel expansion {}x",
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count(),
        result->tile_count,
        result->tile_bytes,
        fixture.server.requests().size(),
        result->tile_count);
}

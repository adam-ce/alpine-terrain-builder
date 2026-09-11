#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <random>
#include "../temporary_directory.h"
#include "Dataset.h"
#include "DatasetReader.h"
#include "Mask.h"
#include "vector_mask.h"
#include "raster_store/storage.h"

namespace {
void write_mask(const std::filesystem::path& path, const std::vector<std::string>& polygons)
{
    auto* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
    REQUIRE(driver);
    Dataset dataset(driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
    OGRSpatialReference reference;
    REQUIRE(reference.importFromEPSG(3857) == OGRERR_NONE);
    auto* layer = dataset.gdalDataset()->CreateLayer("mask", &reference);
    REQUIRE(layer);
    for (const auto& polygon : polygons) {
        OGRGeometry* geometry = nullptr;
        REQUIRE(OGRGeometryFactory::createFromWkt(polygon.c_str(), nullptr, &geometry) == OGRERR_NONE);
        std::unique_ptr<OGRGeometry> owned(geometry);
        OGRFeature feature(layer->GetLayerDefn());
        REQUIRE(feature.SetGeometry(geometry) == OGRERR_NONE);
        REQUIRE(layer->CreateFeature(&feature) == OGRERR_NONE);
    }
}

std::vector<std::uint8_t> reference_selection(const std::string& path, const std::vector<glm::dvec2>& centres,
    std::vector<std::uint8_t> validity)
{
    auto dataset = Dataset::open_vector(path);
    REQUIRE(dataset);
    auto loaded = vector_mask::load_referenced_from_dataset(*dataset);
    REQUIRE(loaded);
    OGRSpatialReference mercator;
    REQUIRE(mercator.importFromEPSG(3857) == OGRERR_NONE);
    mercator.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    std::unique_ptr<OGRCoordinateTransformation> transform(OGRCreateCoordinateTransformation(&mercator, &loaded->srs));
    REQUIRE(transform);
    for (std::size_t i = 0; i < centres.size(); ++i) {
        if (!validity[i]) { continue; }
        auto position = centres[i];
        REQUIRE(transform->Transform(1, &position.x, &position.y));
        const vector_mask::Point2 point(position.x, position.y);
        bool accepted = false;
        for (const auto& polygon : loaded->polygons.polygons_with_holes()) {
            const auto box = polygon.bbox();
            if (position.x < box.xmin() || position.x > box.xmax() || position.y < box.ymin() || position.y > box.ymax()) { continue; }
            if (CGAL::bounded_side_2(polygon.outer_boundary().begin(), polygon.outer_boundary().end(), point) == CGAL::ON_UNBOUNDED_SIDE) { continue; }
            const bool in_hole = std::ranges::any_of(polygon.holes(), [&](const auto& hole) {
                return CGAL::bounded_side_2(hole.begin(), hole.end(), point) == CGAL::ON_BOUNDED_SIDE;
            });
            if (!in_hole) { accepted = true; break; }
        }
        validity[i] = accepted;
    }
    return validity;
}
}

TEST_CASE("RF mask batches agree with ring tests across holes overlaps and shared edges", "[rf-builder][mask]")
{
    GDALAllRegister();
    test::TemporaryDirectory directory;
    const auto path = directory.path() / "mask.geojson";
    write_mask(path, {
        "POLYGON ((0 0,100 0,100 100,0 100,0 0),(30 30,30 70,70 70,70 30,30 30))",
        "POLYGON ((60 40,120 40,120 80,60 80,60 40))",
        "POLYGON ((120 40,150 40,150 80,120 80,120 40))",
        "POLYGON ((160 0,200 0,200 100,190 100,190 10,170 10,170 100,160 100,160 0))" });
    auto loaded = rf_builder::Mask::open(path.string());
    REQUIRE(loaded);
    std::vector<glm::dvec2> points { { 0, 0 }, { 30, 50 }, { 50, 50 }, { 70, 50 }, { 120, 60 }, { 180, 50 }, { 200, 100 } };
    std::mt19937 random(7193);
    std::uniform_real_distribution<double> x(-10, 210), y(-10, 110);
    for (unsigned i = 0; i < 2048; ++i) { points.emplace_back(x(random), y(random)); }
    std::vector<std::uint8_t> validity(points.size(), 1);
    for (std::size_t i = 7; i < validity.size(); i += 13) { validity[i] = 0; }
    const auto expected = reference_selection(path.string(), points, validity);
    REQUIRE(loaded->select(points, validity));
    CHECK(validity == expected);
    // A hole wholly inside the query box must prevent bulk acceptance.
    std::vector<glm::dvec2> enclosed_hole(64, { 50, 50 });
    enclosed_hole.front() = { 10, 10 };
    enclosed_hole.back() = { 90, 90 };
    std::vector<std::uint8_t> enclosed_validity(64, 1);
    REQUIRE(loaded->select(enclosed_hole, enclosed_validity));
    CHECK(enclosed_validity == reference_selection(path.string(), enclosed_hole, std::vector<std::uint8_t>(64, 1)));
    // Uniform spans normalize nonzero validity without reviving invalid pixels.
    std::vector<glm::dvec2> interior(64, { 10, 10 });
    std::vector<std::uint8_t> interior_validity(64, 255);
    interior_validity[20] = 0;
    REQUIRE(loaded->select(interior, interior_validity));
    CHECK(interior_validity[20] == 0);
    CHECK(std::ranges::count(interior_validity, std::uint8_t(1)) == 63);
    // Ordered rows exercise long interior/exterior spans and crossings through holes.
    for (double row : { -1., 0., 1., 30., 40., 50., 70., 80., 99., 100., 101. }) {
        points.clear();
        for (unsigned column = 0; column < 4096; ++column) { points.emplace_back(-10 + column * 220.0 / 4095, row); }
        validity.assign(points.size(), 1);
        const auto row_expected = reference_selection(path.string(), points, validity);
        REQUIRE(loaded->select(points, validity));
        CHECK(validity == row_expected);
    }
    std::vector<glm::dvec2> invalid_points { { NAN, NAN }, { INFINITY, INFINITY } };
    std::vector<std::uint8_t> invalid(2, 0);
    REQUIRE(loaded->select(invalid_points, invalid));
    CHECK(invalid == std::vector<std::uint8_t> { 0, 0 });
    REQUIRE(loaded->select({}, {}));
    CHECK_FALSE(loaded->select(points, {}));
    auto moved = std::move(*loaded);
    interior_validity.assign(64, 1);
    REQUIRE(moved.select(interior, interior_validity));
    CHECK(std::ranges::all_of(interior_validity, [](auto value) { return value == 1; }));
}

TEST_CASE("RF mask benchmark on a supplied vector mask", "[.][rf-mask-benchmark]")
{
    const char* path = std::getenv("ALP_RF_MASK_BENCHMARK");
    if (!path) { SKIP("Set ALP_RF_MASK_BENCHMARK to the original Vienna mask"); }
    auto mask = rf_builder::Mask::open(path);
    REQUIRE(mask);
    REQUIRE(mask->bounds().size() == 1);
    const auto bounds = mask->bounds().front();
    std::vector<glm::dvec2> points;
    for (unsigned row = 0; row < 64; ++row) {
        for (unsigned column = 0; column < 64; ++column) {
            points.emplace_back(bounds.min.x + (column + 0.5) * bounds.width() / 64,
                bounds.min.y + (row + 0.5) * bounds.height() / 64);
        }
    }
    std::vector<std::uint8_t> validity(points.size(), 1);
    REQUIRE(mask->select(points, validity));
    const auto expected = reference_selection(path, points, std::vector<std::uint8_t>(points.size(), 1));
    CHECK(validity == expected);

    BENCHMARK_ADVANCED("RF mask opening")(Catch::Benchmark::Chronometer meter)
    {
        // Retain each result until measurement ends to exclude mask destruction.
        std::vector<std::optional<Expected<rf_builder::Mask>>> opened(meter.runs());
        meter.measure([&](int iteration) { return bool(opened[iteration].emplace(rf_builder::Mask::open(path))); });
        for (const auto& result : opened) {
            REQUIRE(result);
            REQUIRE(*result);
        }
    };

    BENCHMARK_ADVANCED("RF mask selection of 4096 points")(Catch::Benchmark::Chronometer meter)
    {
        // Selection mutates validity; allocate fresh input for every timed run.
        std::vector<std::vector<std::uint8_t>> selections(meter.runs(), std::vector<std::uint8_t>(points.size(), 1));
        std::vector<Expected<void>> results(meter.runs());
        meter.measure([&](int iteration) {
            results[iteration] = mask->select(points, selections[iteration]);
            return bool(results[iteration]);
        });
        for (std::size_t iteration = 0; iteration < results.size(); ++iteration) {
            REQUIRE(results[iteration]);
            CHECK(selections[iteration] == expected);
        }
    };
}

// Keep this as a one-pass serial phase diagnostic: repeated reads warm caches,
// and repeated saves require fresh storage. It bypasses TileWorker/TilePool and
// still packs empty tiles, so it does not measure production import throughput.
TEST_CASE("RF serial tile phase timings on a supplied Vienna DSM", "[.][rf-tile-benchmark]")
{
    const char* mask_path = std::getenv("ALP_RF_MASK_BENCHMARK");
    const char* dataset_path = std::getenv("ALP_RF_TILE_BENCHMARK");
    if (!mask_path || !dataset_path) { SKIP("Set ALP_RF_MASK_BENCHMARK and ALP_RF_TILE_BENCHMARK"); }
    auto mask = rf_builder::Mask::open(mask_path);
    REQUIRE(mask);
    auto dataset = Dataset::open_raster(dataset_path);
    REQUIRE(dataset);
    auto transform = RasterTransform::create(*dataset->gdalDataset());
    REQUIRE(transform);
    test::TemporaryDirectory directory("rf-tile-benchmark");
    {
        std::ofstream attribution(directory.path() / raster_store::attribution::file_name);
        const std::string entity = R"({"spatial_resolution":1,"acquisition_date":"","ingestion_date":"","copyright":"","copyright_link":"","license":""})";
        attribution << "[" << entity << "," << entity << "]";
        attribution.close();
        REQUIRE(attribution.good());
    }
    auto output = raster_store::storage::create<float>(directory.path() / "tiles");
    REQUIRE(output);
    constexpr unsigned side = 4096;
    // Western boundary, central city, and eastern exterior at the planned zoom.
    for (const auto key : { radix::tile::Id { 13, { 4464, 2840 } }, { 13, { 4468, 2840 } }, { 13, { 4473, 2840 } } }) {
        const auto started = std::chrono::steady_clock::now();
        const auto bounds = RasterTransform::tile_bounds(key);
        auto samples = DatasetReader::read_scalar(*dataset->gdalDataset(), *transform, bounds, side, 1);
        REQUIRE(samples);
        const auto read = std::chrono::steady_clock::now();
        std::vector<glm::dvec2> centres(side);
        for (unsigned row = 0; row < side; ++row) {
            for (unsigned column = 0; column < side; ++column) {
                centres[column] = { bounds.min.x + (column + 0.5) * bounds.width() / side,
                    bounds.max.y - (row + 0.5) * bounds.height() / side };
            }
            REQUIRE(mask->select(centres, std::span(samples->valid.buffer()).subspan(std::size_t(row) * side, side)));
        }
        const auto masked = std::chrono::steady_clock::now();
        raster_store::Tile<float> tile(side);
        tile.data = std::move(samples->data);
        for (std::size_t i = 0; i < samples->valid.buffer().size(); ++i) { tile.source_attribution.buffer()[i] = samples->valid.buffer()[i] ? 1 : 0; }
        const auto accepted = std::ranges::count(samples->valid.buffer(), std::uint8_t(1));
        if (accepted) { REQUIRE(output->save(key, tile)); }
        const auto written = std::chrono::steady_clock::now();
        fmt::print("TILE_BENCHMARK key={} accepted={} read_ms={} mask_ms={} pack_write_ms={}\n", to_string(key), accepted,
            std::chrono::duration<double, std::milli>(read - started).count(),
            std::chrono::duration<double, std::milli>(masked - read).count(),
            std::chrono::duration<double, std::milli>(written - masked).count());
        std::fflush(stdout);
    }
}

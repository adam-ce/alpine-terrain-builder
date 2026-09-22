#include "../temporary_directory.h"
#include "Dataset.h"
#include "DatasetReader.h"
#include "Mask.h"
#include "gdal/build.h"
#include "gdal/inputs.h"
#include "gdal/planning.h"
#include "init.h"
#include "io/bytes.h"
#include "raster_store/storage.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <fstream>
#include <ogrsf_frmts.h>
#include <set>
#include <vrtdataset.h>

namespace {
using Bounds = RasterTransform::Bounds;
using Key = radix::tile::Id;
namespace storage = raster_store::storage;
namespace inputs = rf_builder::gdal::inputs;

void write_text(const std::filesystem::path& path, const std::string& text)
{
    REQUIRE(io::write_bytes_to_path(std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()), path));
}

void table(const std::filesystem::path& directory)
{
    const std::string entity = R"({"spatial_resolution":1,"acquisition_date":"2026","ingestion_date":"today","copyright":"test","copyright_link":"https://example.org","license":"test"})";
    write_text(directory / raster_store::attribution::file_name, "[" + entity + "," + entity + "]");
}

Dataset raster(const std::filesystem::path& path, const unsigned side, const unsigned bands,
    std::array<double, 6> affine, int epsg = 3857, GDALDataType type = GDT_Float32)
{
    initialize_gdal_once();
    auto* driver = GetGDALDriverManager()->GetDriverByName(path.empty() ? "MEM" : "GTiff");
    REQUIRE(driver);
    CPLStringList creation_options;
    if (!path.empty() && bands == 3 && type == GDT_Byte) {
        creation_options.SetNameValue("TILED", "YES");
        creation_options.SetNameValue("BLOCKXSIZE", "16");
        creation_options.SetNameValue("BLOCKYSIZE", "16");
        creation_options.SetNameValue("COMPRESS", "JPEG");
        creation_options.SetNameValue("PHOTOMETRIC", "YCBCR");
    }
    Dataset result(driver->Create(path.c_str(), int(side), int(side), int(bands), type, creation_options.List()));
    OGRSpatialReference reference;
    REQUIRE(reference.importFromEPSG(epsg) == OGRERR_NONE);
    reference.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    REQUIRE(result.gdalDataset()->SetSpatialRef(&reference) == CE_None);
    REQUIRE(result.gdalDataset()->SetGeoTransform(affine.data()) == CE_None);
    for (unsigned band = 1; band <= bands; ++band) {
        auto* output = result.gdalDataset()->GetRasterBand(int(band));
        REQUIRE(output->Fill(10 * band) == CE_None);
        if (bands == 3) {
            REQUIRE(output->SetColorInterpretation(std::array { GCI_RedBand, GCI_GreenBand, GCI_BlueBand }[band - 1]) == CE_None);
        }
    }
    return result;
}

std::array<double, 6> affine_for(const Bounds& bounds, unsigned side)
{
    return { bounds.min.x, bounds.width() / side, 0, bounds.max.y, 0, -bounds.height() / side };
}

void mask(const std::filesystem::path& path, const std::string& wkt, int epsg = 3857)
{
    initialize_gdal_once();
    auto* driver = GetGDALDriverManager()->GetDriverByName("GPKG");
    REQUIRE(driver);
    Dataset dataset(driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
    OGRSpatialReference reference;
    REQUIRE(reference.importFromEPSG(epsg) == OGRERR_NONE);
    reference.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    auto* layer = dataset.gdalDataset()->CreateLayer("mask", &reference, wkbUnknown);
    REQUIRE(layer);
    OGRGeometry* geometry = nullptr;
    REQUIRE(OGRGeometryFactory::createFromWkt(wkt.c_str(), nullptr, &geometry) == OGRERR_NONE);
    std::unique_ptr<OGRGeometry> owned_geometry(geometry);
    OGRFeature feature(layer->GetLayerDefn());
    REQUIRE(feature.SetGeometry(geometry) == OGRERR_NONE);
    REQUIRE(layer->CreateFeature(&feature) == OGRERR_NONE);
}

std::string rectangle(const Bounds& bounds)
{
    return fmt::format("POLYGON (({0} {1},{2} {1},{2} {3},{0} {3},{0} {1}))",
        bounds.min.x, bounds.min.y, bounds.max.x, bounds.max.y);
}

struct Fixture {
    test::TemporaryDirectory directory { "rf-import" };
    Bounds bounds = RasterTransform::tile_bounds({ 3, { 4, 3 } });
    rf_builder::gdal::Options options;
    Fixture(unsigned bands = 1)
    {
        table(directory.path());
        options.dataset = (directory.path() / "source.tif").string();
        options.mask = (directory.path() / "mask.gpkg").string();
        options.output = directory.path() / "result";
        options.attribution_index = 1;
        options.tile_side = 16;
        options.mode = bands == 1 ? rf_builder::gdal::Mode::Scalar : rf_builder::gdal::Mode::Colour;
        { auto dataset = raster(options.dataset, 32, bands, affine_for(bounds, 32), 3857, bands == 3 ? GDT_Byte : GDT_Float32); }
        mask(options.mask, rectangle(bounds));
    }
};

std::vector<Key> physical_keys(const auto& snapshot)
{
    std::vector<Key> result;
    for (const auto& [key, status] : snapshot.index()) {
        if (status != store::NodeStatus::Virtual) { result.push_back(key); }
    }
    return result;
}
}

TEST_CASE("RF scalar and RGB snapshots publish disjoint attributed tiles", "[rf-builder]")
{
    const unsigned bands = GENERATE(1u, 3u);
    Fixture fixture(bands);
    fixture.options.jobs = GENERATE(1u, 2u, 4u);
    auto built = rf_builder::gdal::build(fixture.options);
    INFO((built ? "" : built.error().to_string()));
    REQUIRE(built);
    CHECK(built->tile_count == 4);
    CHECK(built->tile_bytes > 0);
    CHECK(built->reused_tiles == 0);
    CHECK_FALSE(std::filesystem::exists(fixture.options.output.string() + ".part"));
    CHECK_FALSE(std::filesystem::exists(fixture.options.output / "inputs.tmp"));
    const auto check = [&](const auto& snapshot) {
        const auto keys = physical_keys(snapshot);
        CHECK(keys.size() == built->tile_count);
        std::uint64_t bytes = 0;
        for (const auto& key : keys) {
            CHECK(key.zoom_level == 4);
            auto tile = snapshot.load(key);
            REQUIRE(tile);
            CHECK(std::ranges::any_of(tile->source_attribution.buffer(), [](auto value) { return value == 1; }));
            CHECK(std::ranges::all_of(tile->source_attribution.buffer(), [](auto value) { return value <= 1; }));
            bytes += std::filesystem::file_size(*snapshot.path_for(key));
        }
        CHECK(bytes == built->tile_bytes);
    };
    if (bands == 1) {
        auto opened_result = storage::open<float>(fixture.options.output);
        REQUIRE(opened_result);
        auto [opened, opened_metadata] = std::move(*opened_result);
        check(*opened);
    } else {
        auto opened_result = storage::open<glm::u8vec3>(fixture.options.output);
        REQUIRE(opened_result);
        auto [opened, opened_metadata] = std::move(*opened_result);
        check(*opened);
    }
}

TEST_CASE("RF reader uses per-channel validity and preserves ordinary black and sentinel values", "[rf-builder]")
{
    const auto bounds = RasterTransform::tile_bounds({ 5, { 16, 15 } });
    auto source = raster({}, 32, 3, affine_for(bounds, 32));
    auto transform = RasterTransform::create(*source.gdalDataset());
    REQUIRE(transform);
    for (int band = 1; band <= 3; ++band) { REQUIRE(source.gdalDataset()->GetRasterBand(band)->Fill(0) == CE_None); }
    auto black = DatasetReader::read_colour(*source.gdalDataset(), *transform, bounds, 32, { 1, 2, 3 });
    REQUIRE(black);
    CHECK(black->valid.buffer()[16 * 32 + 16] != 0);
    CHECK(black->data.buffer()[16 * 32 + 16] == glm::u8vec3(0));
    auto* green = source.gdalDataset()->GetRasterBand(2);
    REQUIRE(green->SetNoDataValue(0) == CE_None);
    auto invalid = DatasetReader::read_colour(*source.gdalDataset(), *transform, bounds, 32, { 1, 2, 3 });
    REQUIRE(invalid);
    CHECK(std::ranges::none_of(invalid->valid.buffer(), [](auto value) { return value != 0; }));
    REQUIRE(green->DeleteNoDataValue() == CE_None);
    REQUIRE(green->CreateMaskBand(0) == CE_None);
    REQUIRE(green->GetMaskBand()->Fill(0) == CE_None);
    auto masked = DatasetReader::read_colour(*source.gdalDataset(), *transform, bounds, 32, { 1, 2, 3 });
    REQUIRE(masked);
    CHECK(std::ranges::none_of(masked->valid.buffer(), [](auto value) { return value != 0; }));
    REQUIRE(source.gdalDataset()->GetRasterBand(1)->Fill(-32768) == CE_None);
    auto ordinary = DatasetReader::read_scalar(*source.gdalDataset(), *transform, bounds, 32, 1);
    REQUIRE(ordinary);
    CHECK(ordinary->valid.buffer()[16 * 32 + 16] != 0);
    CHECK(ordinary->data.buffer()[16 * 32 + 16] == -32768);
}

TEST_CASE("RF mask selection includes boundaries and excludes hole interiors", "[rf-builder]")
{
    test::TemporaryDirectory directory;
    const auto path = directory.path() / "mask.gpkg";
    mask(path, "POLYGON ((0 0,10 0,10 10,0 10,0 0),(3 3,3 7,7 7,7 3,3 3))");
    auto loaded = rf_builder::Mask::open(path.string());
    REQUIRE(loaded);
    const std::array<glm::dvec2, 5> points { glm::dvec2(0, 0), { 3, 5 }, { 5, 5 }, { 2, 2 }, { 11, 5 } };
    std::array<std::uint8_t, 5> validity { 1, 1, 1, 1, 1 };
    REQUIRE(loaded->select(points, validity));
    CHECK(validity == std::array<std::uint8_t, 5> { 1, 1, 0, 1, 0 });
}

TEST_CASE("RF affine transforms support rotation and skew and reject unsupported georeferencing", "[rf-builder]")
{
    auto source = raster({}, 32, 1, { 1000, 10, 2, 2000, 3, -10 });
    auto transform = RasterTransform::create(*source.gdalDataset());
    REQUIRE(transform);
    const glm::dvec2 pixel { 9.5, 18.5 };
    auto position = transform->mercator(pixel);
    REQUIRE(position);
    auto recovered = transform->source_pixel(*position);
    REQUIRE(recovered);
    CHECK(recovered->x == Catch::Approx(pixel.x));
    CHECK(recovered->y == Catch::Approx(pixel.y));
    REQUIRE(source.gdalDataset()->SetMetadataItem("LINE_OFF", "1", "RPC") == CE_None);
    CHECK_FALSE(RasterTransform::create(*source.gdalDataset()));
    REQUIRE(source.gdalDataset()->SetMetadata(nullptr, "RPC") == CE_None);
    REQUIRE(source.gdalDataset()->SetSpatialRef(nullptr) == CE_None);
    CHECK_FALSE(RasterTransform::create(*source.gdalDataset()));
}

TEST_CASE("RF planning measures directional stretch and chooses disjoint mixed zooms", "[rf-builder]")
{
    using namespace rf_builder::gdal::planning;
    CHECK(directional_stretch({ 0, 1 }, { -1, 0 }) == Catch::Approx(1));
    CHECK(directional_stretch({ 2, 0 }, { 0, 0.5 }) == Catch::Approx(2));
    const auto world = RasterTransform::tile_bounds({ 0, { 0, 0 } });
    const double half = RasterTransform::world_half_extent;
    std::vector<Key> selected;
    REQUIRE(traverse(16, { world }, { world }, [half](glm::dvec2 point) -> Expected<glm::dvec2> {
        return glm::dvec2(point.x / half * (point.x < 0 ? 4 : 64), point.y / half * 4);
    }, [&](const Key& key) -> Expected<void> { selected.push_back(key); return {}; }));
    REQUIRE_FALSE(selected.empty());
    std::set<unsigned> levels;
    for (const auto& key : selected) {
        levels.insert(key.zoom_level);
        auto parent = raster_store::StoreTraits::parent(key);
        while (parent) {
            CHECK(std::ranges::find(selected, *parent) == selected.end());
            parent = raster_store::StoreTraits::parent(*parent);
        }
    }
    CHECK(levels.size() >= 2);
    CHECK(RasterTransform::tile_bounds({ 32, { UINT32_MAX, UINT32_MAX } }).max.x == half);
}

TEST_CASE("RF planning retains narrow masks until the selected resolution", "[rf-builder]")
{
    const auto bounds = RasterTransform::tile_bounds({ 3, { 4, 3 } });
    const Bounds narrow { bounds.min + glm::dvec2(1, 1), bounds.min + glm::dvec2(2, 2) };
    std::vector<Key> selected;
    REQUIRE(rf_builder::gdal::planning::traverse(
        16,
        { bounds },
        { narrow },
        [](glm::dvec2 point) -> Expected<glm::dvec2> { return point; },
        [&](const Key& key) -> Expected<void> {
            selected.push_back(key);
            return {};
        }));
    CHECK_FALSE(selected.empty());
    CHECK(selected.front().zoom_level > 3);
}

TEST_CASE("RF antimeridian source reaches canonical tiles on both sides", "[rf-builder]")
{
    Fixture fixture;
    { auto source = raster(fixture.options.dataset, 32, 1, { 170, 0.625, 0, 5, 0, -0.3125 }, 4326); }
    std::filesystem::remove(fixture.options.mask);
    mask(fixture.options.mask, "MULTIPOLYGON (((170 -5,180 -5,180 5,170 5,170 -5)),((-180 -5,-170 -5,-170 5,-180 5,-180 -5)))", 4326);
    auto built = rf_builder::gdal::build(fixture.options);
    INFO((built ? "" : built.error().to_string()));
    REQUIRE(built);
    auto opened_result = storage::open<float>(fixture.options.output);
    REQUIRE(opened_result);
    auto [opened, opened_metadata] = std::move(*opened_result);
    bool west = false, east = false;
    for (const auto& key : physical_keys(*opened)) {
        CHECK(raster_store::StoreTraits::is_valid(key));
        west = west || key.coords.x == 0;
        east = east || key.coords.x == (std::uint64_t(1) << key.zoom_level) - 1;
    }
    CHECK(west);
    CHECK(east);
}

TEST_CASE("RF empty and polar-only coverage publishes an empty snapshot", "[rf-builder]")
{
    Fixture fixture;
    SECTION("source nodata") {
        // Recreate in update-capable form to set nodata.
        auto source = raster(fixture.options.dataset, 32, 1, affine_for(fixture.bounds, 32));
        REQUIRE(source.gdalDataset()->GetRasterBand(1)->SetNoDataValue(10) == CE_None);
    }
    SECTION("polar cutoff") {
        { auto source = raster(fixture.options.dataset, 32, 1, { 10, 0.1, 0, 89, 0, -0.02 }, 4326); }
        std::filesystem::remove(fixture.options.mask);
        mask(fixture.options.mask, "POLYGON ((10 86,14 86,14 89,10 89,10 86))", 4326);
    }
    auto built = rf_builder::gdal::build(fixture.options);
    INFO((built ? "" : built.error().to_string()));
    REQUIRE(built);
    CHECK(built->tile_count == 0);
    CHECK(built->tile_bytes == 0);
    CHECK(storage::open<float>(fixture.options.output));
}

TEST_CASE("RF rejects bad attribution and requested caches before tile production", "[rf-builder]")
{
    Fixture fixture;
    SECTION("zero attribution") { fixture.options.attribution_index = 0; }
    SECTION("zero workers") { fixture.options.jobs = 0; }
    SECTION("unsupported attribution") { fixture.options.attribution_index = 65535; }
    SECTION("absent attribution") { fixture.options.attribution_index = 2; }
    SECTION("missing record") { fixture.options.cache = fixture.directory.path() / "missing.part"; }
    SECTION("corrupt record") {
        fixture.options.cache = fixture.directory.path() / "bad.part";
        write_text(*fixture.options.cache / "inputs.tmp", "bad envelope");
    }
    SECTION("invalid band") { fixture.options.bands = { 0 }; }
    CHECK_FALSE(rf_builder::gdal::build(fixture.options));
    CHECK_FALSE(std::filesystem::exists(fixture.options.output));
    CHECK_FALSE(std::filesystem::exists(fixture.options.output.string() + ".part"));
}

TEST_CASE("RF cache reuse hard-links indexed tiles and ignores unindexed files", "[rf-builder]")
{
    Fixture fixture;
    auto first = rf_builder::gdal::build(fixture.options);
    REQUIRE(first);
    auto original_result = storage::open<float>(fixture.options.output);
    REQUIRE(original_result);
    auto [original, original_metadata] = std::move(*original_result);
    const auto keys = physical_keys(*original);
    REQUIRE(keys.size() == 4);
    const auto cache_path = fixture.directory.path() / "cache";
    {
        storage::CreateOptions create_options;
        create_options.nominal_tile_size = 16;
        auto cache_result = storage::create<float>(cache_path, create_options);
        REQUIRE(cache_result);
        auto [cache, cache_metadata] = std::move(*cache_result);
        REQUIRE(cache->copy_from(keys[0], *original));
        REQUIRE(cache->save_index());
        auto selected_table = raster_store::attribution::read_table(cache->base_path() / "raster_store.index");
        REQUIRE(selected_table);
        const inputs::Record record { *inputs::identifier(fixture.options.dataset),
            *inputs::identifier(fixture.options.mask),
            { 1 },
            rf_builder::gdal::Mode::Scalar,
            1,
            16,
            selected_table->entities[1] };
        REQUIRE(io::envelope::write_to_path<inputs::Schema>(record, cache->base_path() / "inputs.tmp"));
        // An unindexed garbage payload must not be considered reusable.
        write_text(*cache->path_for(keys[1]), "unfinished tile");
    }
    fixture.options.jobs = 4;
    fixture.options.cache = cache_path.string() + ".part";
    fixture.options.output = fixture.directory.path() / "second";
    SECTION("reuse") {
        auto second = rf_builder::gdal::build(fixture.options);
        INFO((second ? "" : second.error().to_string()));
        REQUIRE(second);
        CHECK(second->tile_count == first->tile_count);
        CHECK(second->reused_tiles == 1);
        auto output_result = storage::open<float>(fixture.options.output);
        auto cache_result = storage::open<float>(*fixture.options.cache, { .allow_incomplete = true });
        REQUIRE(output_result);
        auto [output, output_metadata] = std::move(*output_result);
        REQUIRE(cache_result);
        auto [cache, cache_metadata] = std::move(*cache_result);
        CHECK(std::filesystem::equivalent(*output->path_for(keys[0]), *cache->path_for(keys[0])));
    }
    SECTION("equivalent explicit mapping reuses cache")
    {
        fixture.options.value_mapping = raster_store::pixel::Mapping::Linear;
        auto second = rf_builder::gdal::build(fixture.options);
        REQUIRE(second);
        CHECK(second->reused_tiles == 1);
    }
    SECTION("different mapping rejects cache")
    {
        fixture.options.value_mapping = raster_store::pixel::Mapping::SRGBA;
        CHECK_FALSE(rf_builder::gdal::build(fixture.options));
        CHECK_FALSE(std::filesystem::exists(fixture.options.output.string() + ".part"));
    }
    SECTION("mismatched metadata rejects cache")
    {
        auto metadata = raster_store::io::manifest::read_metadata(*fixture.options.cache).value();
        metadata.value_mapping = raster_store::pixel::Mapping::SRGBA;
        REQUIRE(raster_store::io::manifest::write_metadata(metadata, *fixture.options.cache));
        CHECK_FALSE(rf_builder::gdal::build(fixture.options));
        CHECK_FALSE(std::filesystem::exists(fixture.options.output.string() + ".part"));
    }
    SECTION("mismatching record") {
        fixture.options.tile_side = 8;
        CHECK_FALSE(rf_builder::gdal::build(fixture.options));
        CHECK_FALSE(std::filesystem::exists(fixture.options.output.string() + ".part"));
    }
    SECTION("missing indexed payload aborts without indexing the failed link") {
        auto cache_result = storage::open<float>(*fixture.options.cache, { .allow_incomplete = true });
        REQUIRE(cache_result);
        auto [cache, cache_metadata] = std::move(*cache_result);
        REQUIRE(std::filesystem::remove(*cache->path_for(keys[0])));
        auto failed = rf_builder::gdal::build(fixture.options);
        CHECK_FALSE(failed);
        auto partial_result = storage::open<float>(fixture.options.output.string() + ".part", { .allow_incomplete = true });
        REQUIRE(partial_result);
        auto [partial, partial_metadata] = std::move(*partial_result);
        CHECK_FALSE(*partial->has(keys[0]));
        CHECK(std::filesystem::exists(partial->base_path() / "inputs.tmp"));
    }
    SECTION("published snapshot rejected") {
        fixture.options.cache = fixture.directory.path() / "result";
        CHECK_FALSE(rf_builder::gdal::build(fixture.options));
    }
}

TEST_CASE("RF imports a prepared VRT across a file boundary using base-resolution samples", "[rf-builder]")
{
    test::TemporaryDirectory directory;
    const Bounds bounds { { 0, 0 }, { 32, 16 } };
    {
        auto left = raster(directory.path() / "left.tif", 16, 1, { 0, 1, 0, 16, 0, -1 });
        auto right = raster(directory.path() / "right.tif", 16, 1, { 16, 1, 0, 16, 0, -1 });
        REQUIRE(right.gdalDataset()->GetRasterBand(1)->Fill(50) == CE_None);
        REQUIRE(left.gdalDataset()->FlushCache() == CE_None);
        REQUIRE(right.gdalDataset()->FlushCache() == CE_None);
    }
    const auto vrt_path = directory.path() / "mosaic.vrt";
    write_text(vrt_path, fmt::format(R"(<VRTDataset rasterXSize="32" rasterYSize="16">
<SRS>EPSG:3857</SRS><GeoTransform>0,1,0,16,0,-1</GeoTransform>
<VRTRasterBand dataType="Float32" band="1">
<SimpleSource><SourceFilename>{}</SourceFilename><SourceBand>1</SourceBand><SrcRect xOff="0" yOff="0" xSize="16" ySize="16"/><DstRect xOff="0" yOff="0" xSize="16" ySize="16"/></SimpleSource>
<SimpleSource><SourceFilename>{}</SourceFilename><SourceBand>1</SourceBand><SrcRect xOff="0" yOff="0" xSize="16" ySize="16"/><DstRect xOff="16" yOff="0" xSize="16" ySize="16"/></SimpleSource>
</VRTRasterBand></VRTDataset>)", (directory.path() / "left.tif").string(), (directory.path() / "right.tif").string()));
    Dataset mosaic(vrt_path);
    auto transform = RasterTransform::create(*mosaic.gdalDataset());
    REQUIRE(transform);
    const Bounds window { { 8.25, 0.25 }, { 24.25, 16.25 } };
    auto combined = DatasetReader::read_scalar(*mosaic.gdalDataset(), *transform, window, 16, 1);
    REQUIRE(combined);
    auto reference = raster({}, 32, 1, { 0, 1, 0, 16, 0, -1 });
    std::vector<float> values(32 * 32, 10);
    for (unsigned y = 0; y < 32; ++y) {
        for (unsigned x = 16; x < 32; ++x) { values[y * 32 + x] = 50; }
    }
    REQUIRE(reference.gdalDataset()->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, 32, 32, values.data(), 32, 32, GDT_Float32, 0, 0) == CE_None);
    auto reference_transform = RasterTransform::create(*reference.gdalDataset());
    REQUIRE(reference_transform);
    auto expected = DatasetReader::read_scalar(*reference.gdalDataset(), *reference_transform, window, 16, 1);
    REQUIRE(expected);
    // Compare our source assembly, not GDAL's filter formula.
    for (unsigned x = 0; x < 16; ++x) {
        CHECK(combined->data.buffer()[8 * 16 + x] == Catch::Approx(expected->data.buffer()[8 * 16 + x]));
    }
    const int level = 2;
    REQUIRE(reference.gdalDataset()->BuildOverviews("NEAREST", 1, &level, 0, nullptr, nullptr, nullptr) == CE_None);
    REQUIRE(reference.gdalDataset()->GetRasterBand(1)->GetOverview(0)->Fill(222) == CE_None);
    auto base = DatasetReader::read_scalar(*reference.gdalDataset(), *reference_transform,
        { { 0, -16 }, { 32, 16 } }, 16, 1);
    REQUIRE(base);
    CHECK(base->data.buffer()[8 * 16 + 4] != 222);
}

TEST_CASE("RF filtering agrees across output windows and a longitude seam", "[rf-builder]")
{
    auto source = raster({}, 64, 1, { 170, 20. / 64, 0, 10, 0, -20. / 64 }, 4326);
    std::vector<float> values(64 * 64);
    for (unsigned y = 0; y < 64; ++y) {
        for (unsigned x = 0; x < 64; ++x) { values[y * 64 + x] = float(x); }
    }
    REQUIRE(source.gdalDataset()->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, 64, 64, values.data(), 64, 64, GDT_Float32, 0, 0) == CE_None);
    auto transform = RasterTransform::create(*source.gdalDataset());
    REQUIRE(transform);
    const double half = RasterTransform::world_half_extent;
    const double width = 2 * half / 64;
    const Bounds east { { half - width, -width / 2 }, { half, width / 2 } };
    const Bounds west { { -half, -width / 2 }, { -half + width, width / 2 } };
    const Bounds across { { half - width, -width }, { half + width, width } };
    auto right_edge = DatasetReader::read_scalar(*source.gdalDataset(), *transform, east, 16, 1);
    auto left_edge = DatasetReader::read_scalar(*source.gdalDataset(), *transform, west, 16, 1);
    auto joined = DatasetReader::read_scalar(*source.gdalDataset(), *transform, across, 32, 1);
    REQUIRE(right_edge);
    REQUIRE(left_edge);
    REQUIRE(joined);
    for (unsigned row = 0; row < 16; ++row) {
        for (unsigned column = 0; column < 16; ++column) {
            CHECK(right_edge->data.buffer()[row * 16 + column] == Catch::Approx(joined->data.buffer()[(row + 8) * 32 + column]).margin(1e-4));
            CHECK(left_edge->data.buffer()[row * 16 + column] == Catch::Approx(joined->data.buffer()[(row + 8) * 32 + column + 16]).margin(1e-4));
            CHECK(right_edge->valid.buffer()[row * 16 + column] != 0);
            CHECK(left_edge->valid.buffer()[row * 16 + column] != 0);
        }
    }
}

TEST_CASE("RF mask is an output selection and not a source cutline", "[rf-builder]")
{
    Fixture fixture;
    const double spacing = fixture.bounds.width() / 32;
    const Bounds tiny { fixture.bounds.min + glm::dvec2(15 * spacing, 15 * spacing),
        fixture.bounds.min + glm::dvec2(17 * spacing, 17 * spacing) };
    std::filesystem::remove(fixture.options.mask);
    mask(fixture.options.mask, rectangle(tiny));
    auto built = rf_builder::gdal::build(fixture.options);
    REQUIRE(built);
    auto opened_result = storage::open<float>(fixture.options.output);
    REQUIRE(opened_result);
    auto [opened, opened_metadata] = std::move(*opened_result);
    unsigned accepted = 0;
    for (const auto& key : physical_keys(*opened)) {
        auto tile = opened->load(key);
        REQUIRE(tile);
        for (std::size_t i = 0; i < tile->data.buffer().size(); ++i) {
            if (tile->source_attribution.buffer()[i]) {
                ++accepted;
                CHECK(tile->data.buffer()[i] == Catch::Approx(10));
            }
        }
    }
    CHECK(accepted == 4);
}

TEST_CASE("rf-builder command reports a published snapshot and rejects invalid bands", "[rf-builder][cli]")
{
    Fixture fixture;
    unsigned expected_tiles = 4;
    unsigned expected_candidates = 4;
    SECTION("populated snapshot") { }
    SECTION("all candidates have no valid pixels") {
        Dataset source(static_cast<GDALDataset*>(GDALOpen(fixture.options.dataset.c_str(), GA_Update)));
        REQUIRE(source.gdalDataset());
        REQUIRE(source.gdalDataset()->GetRasterBand(1)->SetNoDataValue(10) == CE_None);
        expected_tiles = 0;
    }
    SECTION("no candidates intersect the mask") {
        fixture.options.mask = (fixture.directory.path() / "outside.gpkg").string();
        mask(fixture.options.mask, rectangle(RasterTransform::tile_bounds({ 3, { 1, 1 } })));
        expected_tiles = 0;
        expected_candidates = 0;
    }
    const auto quote = [](std::string text) {
        std::string result = "'";
        for (char value : text) { result += value == '\'' ? "'\\''" : std::string(1, value); }
        return result + "'";
    };
    const auto log_path = fixture.directory.path() / "command.log";
    const auto command = quote(ALP_RF_BUILDER_PATH) + " gdal --dataset " + quote(fixture.options.dataset) + " --mask " + quote(fixture.options.mask)
        + " --output " + quote(fixture.options.output.string()) + " --attribution-index 1 --tile-size 16 --mode scalar --jobs 2 > " + quote(log_path.string())
        + " 2>&1";
    REQUIRE(std::system(command.c_str()) == 0);
    std::ifstream log(log_path);
    const std::string text { std::istreambuf_iterator<char>(log), std::istreambuf_iterator<char>() };
    CHECK(text.find(fmt::format("{} tiles, 16x16 pixels per tile", expected_tiles)) != std::string::npos);
    CHECK(text.find("payload bytes") != std::string::npos);
    const auto completed = fmt::format("{0}/{0} candidate tiles (100.0%)", expected_candidates);
    CHECK(text.find(completed) != std::string::npos);
    CHECK(text.find("remaining 0h 00m 00s") != std::string::npos);
    if (expected_candidates != 0) { CHECK(text.find("expected finish") != std::string::npos); }
    std::ifstream persisted_log(fixture.options.output.string() + ".log");
    const std::string persisted_text { std::istreambuf_iterator<char>(persisted_log), std::istreambuf_iterator<char>() };
    CHECK(persisted_text.find("RF import: dataset=") != std::string::npos);
    CHECK(persisted_text.find(completed) != std::string::npos);
    CHECK(persisted_text.find("Published " + fixture.options.output.string()) != std::string::npos);
    CHECK(storage::open<float>(fixture.options.output));
    const auto invalid = quote(ALP_RF_BUILDER_PATH) + " gdal --dataset " + quote(fixture.options.dataset) + " --mask " + quote(fixture.options.mask)
        + " --output " + quote((fixture.directory.path() / "bad").string()) + " --attribution-index 1 --tile-size 16 --bands 0 > " + quote(log_path.string())
        + " 2>&1";
    CHECK(std::system(invalid.c_str()) != 0);
    std::ifstream failure_log(fixture.directory.path() / "bad.log");
    const std::string failure_text { std::istreambuf_iterator<char>(failure_log), std::istreambuf_iterator<char>() };
    CHECK(failure_text.find("source band is out of range") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(fixture.directory.path() / "bad.part"));
}

TEST_CASE("RF imports rotated and skewed grids through the command pipeline", "[rf-builder]")
{
    Fixture fixture;
    const double spacing = fixture.bounds.width() / 32;
    { auto source = raster(fixture.options.dataset, 32, 1,
          { fixture.bounds.min.x, spacing, spacing * 0.2, fixture.bounds.max.y, spacing * 0.3, -spacing }); }
    auto result = rf_builder::gdal::build(fixture.options);
    INFO((result ? "" : result.error().to_string()));
    REQUIRE(result);
    CHECK(result->tile_count > 0);
}

TEST_CASE("RF planning rejects a ratio that cannot fit the maximum supported zoom", "[rf-builder]")
{
    const auto bounds = RasterTransform::tile_bounds({ 32, { 0, 0 } });
    const auto result = rf_builder::gdal::planning::traverse(
        16,
        { bounds },
        { bounds },
        [](glm::dvec2 position) -> Expected<glm::dvec2> { return position * 1e12; },
        [](const Key&) -> Expected<void> {
            FAIL("No tile may be accepted");
            return {};
        });
    REQUIRE_FALSE(result);
    CHECK(result.error().code() == Error::Code::Unsupported);
}

TEST_CASE("RF curved projected coverage retains edge extrema between densification samples", "[rf-builder]")
{
    OGRSpatialReference polar;
    REQUIRE(polar.importFromEPSG(3413) == OGRERR_NONE);
    polar.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    const Bounds rectangle_bounds { { -100000, -2000000 }, { 100000, -1000000 } };
    auto coverage = RasterTransform::coverage(polar, rectangle_bounds);
    REQUIRE(coverage);
    OGRSpatialReference mercator;
    REQUIRE(mercator.importFromEPSG(3857) == OGRERR_NONE);
    mercator.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    std::unique_ptr<OGRCoordinateTransformation> transform(OGRCreateCoordinateTransformation(&polar, &mercator));
    REQUIRE(transform);
    for (int i = 0; i <= 1000; ++i) {
        double x = -100000 + 200.0 * i;
        double y = -1000000;
        REQUIRE(transform->Transform(1, &x, &y));
        CHECK(std::ranges::any_of(*coverage, [&](const auto& bounds) {
            return x >= bounds.min.x && x <= bounds.max.x && y >= bounds.min.y && y <= bounds.max.y;
        }));
    }
}

TEST_CASE("RF ignores GDAL threading environment for its synchronous transformer", "[rf-builder]")
{
    struct Configuration {
        const char* name;
        std::optional<std::string> previous = std::nullopt;
        Configuration(const char* option, const char* value) : name(option)
        {
            if (const auto* old = CPLGetConfigOption(name, nullptr)) { previous = old; }
            CPLSetConfigOption(name, value);
        }
        ~Configuration() { CPLSetConfigOption(name, previous ? previous->c_str() : nullptr); }
    };
    const Configuration threads("GDAL_NUM_THREADS", "2");
    const Configuration chunks("WARP_THREAD_CHUNK_SIZE", "1");
    const auto bounds = RasterTransform::tile_bounds({ 5, { 16, 15 } });
    auto source = raster({}, 32, 1, affine_for(bounds, 32));
    auto transform = RasterTransform::create(*source.gdalDataset());
    REQUIRE(transform);
    CPLErrorReset();
    auto read = DatasetReader::read_scalar(*source.gdalDataset(), *transform, bounds, 32, 1);
    REQUIRE(read);
    CHECK(CPLGetLastErrorType() < CE_Failure);
    CHECK(read->valid.buffer()[16 * 32 + 16] != 0);
}

TEST_CASE("RF excludes invalid projection probes outside a coarse source footprint", "[rf-builder]")
{
    auto source = raster({}, 4, 1, { -3500000, 2000000, 0, 4000000, 0, -2000000 }, 32632);
    auto transform = RasterTransform::create(*source.gdalDataset());
    REQUIRE(transform);
    auto read = DatasetReader::read_scalar(*source.gdalDataset(), *transform,
        RasterTransform::tile_bounds({ 0, { 0, 0 } }), 16, 1);
    INFO((read ? "" : read.error().to_string()));
    REQUIRE(read);
}

TEST_CASE("RF default-size tile can contain a source smaller than GDAL probe spacing", "[rf-builder]")
{
    auto source = raster({}, 32, 1, { 100, 1, 0, 200, 0, -1 });
    auto transform = RasterTransform::create(*source.gdalDataset());
    REQUIRE(transform);
    auto read = DatasetReader::read_scalar(*source.gdalDataset(), *transform, { { 0, 0 }, { 4096, 4096 } }, 4096, 1);
    INFO((read ? "" : read.error().to_string()));
    REQUIRE(read);
    CHECK(read->valid.buffer()[std::size_t(4096 - 184) * 4096 + 116] != 0);
    CHECK(read->data.buffer()[std::size_t(4096 - 184) * 4096 + 116] == Catch::Approx(10));
}

TEST_CASE("RF parallel imports match serial pixels attribution and hierarchy", "[rf-builder][parallel]")
{
    const unsigned bands = GENERATE(1u, 3u);
    Fixture fixture(bands);
    const auto outer = fixture.bounds;
    const auto inner = Bounds { glm::mix(outer.min, outer.max, glm::dvec2(0.25)), glm::mix(outer.min, outer.max, glm::dvec2(0.75)) };
    std::filesystem::remove(fixture.options.mask);
    mask(fixture.options.mask,
        fmt::format("POLYGON (({0} {1},{2} {1},{2} {3},{0} {3},{0} {1}),({4} {5},{4} {7},{6} {7},{6} {5},{4} {5}))",
            outer.min.x,
            outer.min.y,
            outer.max.x,
            outer.max.y,
            inner.min.x,
            inner.min.y,
            inner.max.x,
            inner.max.y));
    REQUIRE(rf_builder::gdal::build(fixture.options));
    const auto baseline = fixture.options.output;
    const auto compare = [&]<typename PixelType>() {
        auto original_result = storage::open<PixelType>(baseline);
        REQUIRE(original_result);
        auto [original, original_metadata] = std::move(*original_result);
        const auto keys = physical_keys(*original);
        for (const unsigned jobs : { 2u, 4u, 8u, 12u }) {
            fixture.options.jobs = jobs;
            fixture.options.output = fixture.directory.path() / ("parallel-" + std::to_string(jobs));
            auto built = rf_builder::gdal::build(fixture.options);
            INFO((built ? "" : built.error().to_string()));
            REQUIRE(built);
            auto output_result = storage::open<PixelType>(fixture.options.output);
            REQUIRE(output_result);
            auto [output, output_metadata] = std::move(*output_result);
            CHECK(physical_keys(*output) == keys);
            for (const auto& key : keys) {
                auto expected = original->load(key);
                auto actual = output->load(key);
                REQUIRE(expected);
                REQUIRE(actual);
                CHECK(std::ranges::equal(actual->data.buffer(), expected->data.buffer()));
                CHECK(std::ranges::equal(actual->source_attribution.buffer(), expected->source_attribution.buffer()));
            }
            for (const auto& entry : std::filesystem::recursive_directory_iterator(baseline)) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                const auto relative = entry.path().lexically_relative(baseline);
                auto expected_bytes = io::read_bytes_from_path(entry.path());
                auto actual_bytes = io::read_bytes_from_path(fixture.options.output / relative);
                REQUIRE(expected_bytes);
                REQUIRE(actual_bytes);
                CHECK(*actual_bytes == *expected_bytes);
            }
        }
    };
    if (bands == 1) {
        compare.template operator()<float>();
    } else {
        compare.template operator()<glm::u8vec3>();
    }
}

TEST_CASE("RF cancellation checkpoints completed work and can reuse it", "[rf-builder][parallel]")
{
    Fixture fixture;
    fixture.options.jobs = 2;
    {
        auto source = raster(fixture.options.dataset, 128, 1, affine_for(fixture.bounds, 128));
    }
    const auto partial_path = std::filesystem::path(fixture.options.output.string() + ".part");
    unsigned polls = 0;
    const auto cancelled = rf_builder::gdal::build(fixture.options, [&] {
        ++polls;
        // Index metadata lives directly in .part; a zoom directory only appears
        // once the coordinator has saved a payload.
        if (!std::filesystem::exists(partial_path)) {
            return false;
        }
        for (const auto& entry : std::filesystem::directory_iterator(partial_path)) {
            if (entry.is_directory()) {
                return true;
            }
        }
        return false;
    });
    REQUIRE_FALSE(cancelled);
    CHECK(cancelled.error().code() == Error::Code::Cancelled);
    CHECK(polls > 0);
    CHECK_FALSE(std::filesystem::exists(fixture.options.output));
    CHECK(std::filesystem::exists(partial_path / "inputs.tmp"));
    auto partial_result = storage::open<float>(partial_path, { .allow_incomplete = true });
    REQUIRE(partial_result);
    auto [partial, partial_metadata] = std::move(*partial_result);
    const auto keys = physical_keys(*partial);
    REQUIRE_FALSE(keys.empty());
    CHECK(keys.size() <= 5); // one consumed tile plus at most 2*jobs outstanding
    for (const auto& key : keys) {
        REQUIRE(partial->load(key));
    }
    fixture.options.output = fixture.directory.path() / "resumed";
    fixture.options.cache = partial_path;
    auto resumed = rf_builder::gdal::build(fixture.options);
    REQUIRE(resumed);
    CHECK(resumed->reused_tiles == keys.size());
    CHECK(resumed->tile_count == 64);
}

TEST_CASE("RF cancellation during planning leaves an empty reusable checkpoint", "[rf-builder][parallel]")
{
    Fixture fixture;
    const auto result = rf_builder::gdal::build(fixture.options, [] { return true; });
    REQUIRE_FALSE(result);
    CHECK(result.error().code() == Error::Code::Cancelled);
    auto partial_result = storage::open<float>(fixture.options.output.string() + ".part", { .allow_incomplete = true });
    REQUIRE(partial_result);
    auto [partial, partial_metadata] = std::move(*partial_result);
    CHECK(physical_keys(*partial).empty());
    CHECK(std::filesystem::exists(partial->base_path() / "inputs.tmp"));
}

TEST_CASE("RF parallel writer failure never publishes or indexes a failed payload", "[rf-builder][parallel]")
{
    Fixture fixture;
    fixture.options.jobs = 4;
    const auto partial_path = std::filesystem::path(fixture.options.output.string() + ".part");
    bool blocked = false;
    const auto result = rf_builder::gdal::build(fixture.options, [&] {
        if (!blocked && std::filesystem::exists(partial_path)) {
            write_text(partial_path / "4", "blocks creation of the zoom directory");
            blocked = true;
        }
        return false;
    });
    REQUIRE(blocked);
    REQUIRE_FALSE(result);
    CHECK_FALSE(std::filesystem::exists(fixture.options.output));
    auto partial_result = storage::open<float>(partial_path, { .allow_incomplete = true });
    REQUIRE(partial_result);
    auto [partial, partial_metadata] = std::move(*partial_result);
    CHECK(physical_keys(*partial).empty());
    CHECK(std::filesystem::exists(partial_path / "inputs.tmp"));
}

TEST_CASE("RF value mapping defaults and overrides persist without changing pixel representation", "[rf-builder]")
{
    const auto bands = GENERATE(1u, 3u);
    Fixture fixture(bands);
    SECTION("default") { }
    SECTION("linear override") { fixture.options.value_mapping = raster_store::pixel::Mapping::Linear; }
    SECTION("srgba override") { fixture.options.value_mapping = raster_store::pixel::Mapping::SRGBA; }
    auto result = rf_builder::gdal::build(fixture.options);
    REQUIRE(result);
    auto metadata = raster_store::io::manifest::read_metadata(fixture.options.output);
    REQUIRE(metadata);
    CHECK(metadata->value_mapping
        == fixture.options.value_mapping.value_or(bands == 3 ? raster_store::pixel::Mapping::SRGBA : raster_store::pixel::Mapping::Linear));
    CHECK(metadata->halo_width == 0);
    CHECK(metadata->nominal_tile_size == 16);
    CHECK(metadata->stored_tile_size == 16);
}

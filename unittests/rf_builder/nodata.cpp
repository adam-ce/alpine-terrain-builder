#include "gdal/nodata.h"
#include "../temporary_directory.h"
#include "Dataset.h"
#include "gdal/cli.h"
#include "gdal/planning.h"
#include "init.h"
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <future>

namespace {
namespace nodata = rf_builder::gdal::nodata;

template <typename T>
DatasetReader::Samples<T> samples(unsigned side, T value)
{
    return { radix::Raster<T>(glm::uvec2(side), value), radix::Raster<std::uint8_t>(glm::uvec2(side), 1) };
}

class CountingBand final : public GDALRasterBand {
public:
    CountingBand(GDALDataset* owner)
    {
        poDS = owner;
        nBand = 1;
        eDataType = GDT_Float32;
        nBlockXSize = 64;
        nBlockYSize = 1;
    }
    int widest_read = 0;
    unsigned read_count = 0;

protected:
    CPLErr IReadBlock(int column, int, void* data) override
    {
        for (int i = 0; i < nBlockXSize; ++i) {
            static_cast<float*>(data)[i] = float(column * nBlockXSize + i);
        }
        return CE_None;
    }
    CPLErr IRasterIO(GDALRWFlag direction,
        int x,
        int y,
        int width,
        int height,
        void* data,
        int output_width,
        int output_height,
        GDALDataType type,
        GSpacing pixel_spacing,
        GSpacing line_spacing,
        GDALRasterIOExtraArg* extra) override
    {
        widest_read = (std::max)(widest_read, width);
        ++read_count;
        return GDALRasterBand::IRasterIO(direction, x, y, width, height, data, output_width, output_height, type, pixel_spacing, line_spacing, extra);
    }
};

class CountingDataset final : public GDALDataset {
public:
    explicit CountingDataset(double west)
        : m_west(west)
    {
        nRasterXSize = 4096;
        nRasterYSize = 256;
        REQUIRE(m_reference.importFromEPSG(3857) == OGRERR_NONE);
        m_reference.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        SetBand(1, new CountingBand(this));
    }
    const OGRSpatialReference* GetSpatialRef() const override { return &m_reference; }
    CPLErr GetGeoTransform(double* affine) override
    {
        const double step = 2 * RasterTransform::world_half_extent / nRasterXSize;
        const std::array<double, 6> values { m_west, step, 0, step * nRasterYSize / 2, 0, -step };
        std::ranges::copy(values, affine);
        return CE_None;
    }

private:
    OGRSpatialReference m_reference;
    double m_west;
};
} // namespace

TEST_CASE("RF NoData settings validate dimensions and CLI fallback forms", "[rf-builder][rf-nodata]")
{
    CHECK(*nodata::halo(4096, 5, 5) == 7);
    CHECK(*nodata::halo(4096, 0, 1) == 0);
    CHECK_FALSE(nodata::halo(16, 5, 0));
    CHECK_FALSE(nodata::halo(16, 5, 4));
    CHECK_FALSE(nodata::halo(16, (std::numeric_limits<unsigned>::max)(), 5));
    CHECK_FALSE(nodata::halo((std::numeric_limits<unsigned>::max)(), 0, 1));
    rf_builder::gdal::Options options;
    CLI::App app;
    rf_builder::gdal::cli::configure(app, options);
    const auto fallback = GENERATE("0", "128", "128,64,32");
    app.parse(std::string("--dataset input --mask mask --output result --attribution-index 1 --mode rgb --nodata-default-value ") + fallback
        + " --nodata-search-radius 0 --nodata-smoothing-kernel-size 1");
    CHECK(options.nodata_search_radius == 0);
    CHECK(options.nodata_smoothing_kernel_size == 1);
    if (std::string_view(fallback).contains(',')) {
        CHECK(options.nodata_default_value == std::vector<double> { 128, 64, 32 });
    } else {
        CHECK(options.nodata_default_value == std::vector<double> { std::stod(fallback) });
    }
}

TEST_CASE("RF NoData preserves valid samples and fills small holes with bounded fallback", "[rf-builder][rf-nodata]")
{
    initialize_gdal_once();
    auto input = samples<float>(64, 100);
    for (unsigned y = 0; y < 64; ++y) {
        for (unsigned x = 24; x < 64; ++x) {
            input.valid.pixel({ x, y }) = 0;
        }
    }
    input.valid.pixel({ 12, 12 }) = 0;
    input.data.pixel({ 11, 12 }) = 0; // Valid zero remains a donor and is preserved.
    input.data.pixel({ 8, 8 }) = -32768;
    auto processor = nodata::Processor::create(5, 5);
    REQUIRE(processor);
    const float fallback = GENERATE(0.f, 12.f);
    auto result = processor->process(input, { 7, 7 }, 48, { fallback, fallback, fallback });
    REQUIRE(result);
    CHECK(result->pixel({ 5, 5 }) > 0);
    CHECK(result->pixel({ 24, 20 }) == fallback);
    CHECK(result->pixel({ 19, 20 }) > fallback);
    for (unsigned y = 0; y < 48; ++y) {
        for (unsigned x = 0; x < 48; ++x) {
            CHECK(std::isfinite(result->pixel({ x, y })));
            if (input.valid.pixel({ x + 7, y + 7 })) {
                CHECK(result->pixel({ x, y }) == input.data.pixel({ x + 7, y + 7 }));
            }
        }
    }
}

TEST_CASE("RF NoData disable modes and RGB fallback preserve originals", "[rf-builder][rf-nodata]")
{
    initialize_gdal_once();
    auto input = samples<glm::u8vec3>(32, { 10, 20, 30 });
    input.valid.pixel({ 15, 15 }) = 0;
    auto processor = nodata::Processor::create(0, 1);
    REQUIRE(processor);
    auto result = processor->process(input, { 7, 7 }, 16, { 128, 64, 32 });
    REQUIRE(result);
    CHECK(result->pixel({ 8, 8 }) == glm::u8vec3(128, 64, 32));
    CHECK(result->pixel({ 7, 8 }) == glm::u8vec3(10, 20, 30));
    auto filled = nodata::Processor::create(5, 1);
    REQUIRE(filled);
    auto interpolated = filled->process(input, { 7, 7 }, 16, { 128, 64, 32 });
    REQUIRE(interpolated);
    CHECK(interpolated->pixel({ 8, 8 }) == glm::u8vec3(10, 20, 30));
    std::ranges::fill(input.valid.buffer(), 0);
    auto smooth = nodata::Processor::create(5, 5);
    REQUIRE(smooth);
    auto empty = smooth->process(input, { 7, 7 }, 16, { 128, 64, 32 });
    REQUIRE(empty);
    CHECK(std::ranges::all_of(empty->buffer(), [](auto value) { return value == glm::u8vec3(128, 64, 32); }));
}

TEST_CASE("RF NoData Gaussian composes both passes before restoring valid values", "[rf-builder][rf-nodata]")
{
    auto input = samples<float>(9, 0);
    input.data.pixel({ 3, 3 }) = 100;
    input.valid.pixel({ 4, 4 }) = 0;
    auto processor = nodata::Processor::create(0, 5);
    REQUIRE(processor);
    auto result = processor->process(input, { 2, 2 }, 5, {});
    REQUIRE(result);
    const auto weights = *raster::algorithm::gaussian_kernel(5);
    CHECK(result->pixel({ 2, 2 }) == Catch::Approx(100 * weights[1] * weights[1]));
    CHECK(result->pixel({ 1, 1 }) == 100);
}

TEST_CASE("RF NoData split windows agree and parallel processors remain independent", "[rf-builder][rf-nodata]")
{
    initialize_gdal_once();
    auto input = samples<float>(78, 80);
    for (unsigned y = 0; y < 78; ++y) {
        for (unsigned x = 0; x < 78; ++x) {
            input.data.pixel({ x, y }) = float(x + 2 * y);
            input.valid.pixel({ x, y }) = (x < 31 || x > 42) && ((x + y) % 11 != 0);
        }
    }
    auto processor = nodata::Processor::create(5, 5);
    REQUIRE(processor);
    auto whole = processor->process(input, { 7, 7 }, 64, {});
    REQUIRE(whole);
    std::vector<std::future<Expected<radix::Raster<float>>>> futures;
    for (unsigned part = 0; part < 4; ++part) {
        futures.push_back(std::async(std::launch::async, [&, part] {
            const glm::uvec2 origin { (part % 2) * 32, (part / 2) * 32 };
            auto piece = samples<float>(46, 0);
            for (unsigned y = 0; y < 46; ++y) {
                for (unsigned x = 0; x < 46; ++x) {
                    piece.data.pixel({ x, y }) = input.data.pixel(origin + glm::uvec2(x, y));
                    piece.valid.pixel({ x, y }) = input.valid.pixel(origin + glm::uvec2(x, y));
                }
            }
            auto worker = nodata::Processor::create(5, 5);
            return worker->process(piece, { 7, 7 }, 32, {});
        }));
    }
    for (unsigned part = 0; part < 4; ++part) {
        auto piece = futures[part].get();
        REQUIRE(piece);
        const glm::uvec2 origin { (part % 2) * 32, (part / 2) * 32 };
        for (unsigned y = 0; y < 32; ++y) {
            for (unsigned x = 0; x < 32; ++x) {
                CHECK(piece->pixel({ x, y }) == Catch::Approx(whole->pixel(origin + glm::uvec2(x, y))).margin(1e-5));
            }
        }
    }
}

TEST_CASE("RF NoData polar windows clip reads and replicate completed borders", "[rf-builder][rf-nodata]")
{
    const auto north = nodata::window({ 2, { 0, 0 } }, 16, 7);
    const auto south = nodata::window({ 2, { 0, 3 } }, 16, 7);
    CHECK(north.size == glm::uvec2(30, 23));
    CHECK(north.interior_offset == glm::uvec2(7, 0));
    CHECK(north.bounds.max.y == RasterTransform::world_half_extent);
    CHECK(south.bounds.min.y == -RasterTransform::world_half_extent);
    auto input = samples<float>(8, 0);
    std::ranges::fill(input.valid.buffer(), 0);
    input.valid.pixel({ 3, 0 }) = 1;
    input.data.pixel({ 3, 0 }) = 100;
    auto processor = nodata::Processor::create(0, 5);
    REQUIRE(processor);
    auto result = processor->process(input, { 2, 0 }, 4, {});
    REQUIRE(result);
    const auto weights = *raster::algorithm::gaussian_kernel(5);
    CHECK(result->pixel({ 2, 0 }) == Catch::Approx(100 * weights[1] * (weights[0] + weights[1] + weights[2])));
}

TEST_CASE("RF periodic seam reads narrow source strips and postprocessing does not reread", "[rf-builder][rf-nodata]")
{
    initialize_gdal_once();
    const double half = RasterTransform::world_half_extent;
    const double west = GENERATE_COPY(-half, 0.);
    CountingDataset dataset(west);
    auto transform = RasterTransform::create(dataset);
    REQUIRE(transform);
    const double step = 2 * half / 4096;
    const RasterTransform::Bounds probe { { -step / 8, -step }, { step, step } };
    auto ratio = rf_builder::gdal::planning::estimate(probe, step, [&](glm::dvec2 point) { return transform->source_pixel(point, false); });
    REQUIRE(ratio);
    CHECK(*ratio == Catch::Approx(1));
    const RasterTransform::Bounds bounds { { west - 16 * step, -16 * step }, { west + 16 * step, 16 * step } };
    auto read = DatasetReader::read_scalar(dataset, *transform, bounds, 32, 1);
    REQUIRE(read);
    auto* band = static_cast<CountingBand*>(dataset.GetRasterBand(1));
    CHECK(band->read_count > 0);
    CHECK(band->widest_read < 128);
    CHECK(std::ranges::all_of(read->valid.buffer(), [](auto value) { return value != 0; }));
    for (unsigned x = 0; x < 32; ++x) {
        CHECK(read->data.pixel({ x, 16 }) == Catch::Approx(float((4096 - 16 + x) % 4096)).margin(0.01));
    }
    const unsigned reads = band->read_count;
    read->valid.pixel({ 15, 15 }) = 0;
    auto processor = nodata::Processor::create(5, 5);
    REQUIRE(processor);
    test::TemporaryDirectory temporary;
    CPLSetThreadLocalConfigOption("CPL_TMPDIR", temporary.path().c_str());
    auto result = processor->process(*read, { 7, 7 }, 16, {});
    CPLSetThreadLocalConfigOption("CPL_TMPDIR", nullptr);
    REQUIRE(result);
    CHECK(band->read_count == reads);
    CHECK(std::filesystem::is_empty(temporary.path()));
    CHECK(std::isfinite(result->pixel({ 8, 8 })));
}

TEST_CASE("RF NoData stage benchmarks", "[!benchmark][rf-nodata]")
{
    initialize_gdal_once();
    const unsigned side = 512;
    auto input = samples<float>(side + 14, 80);
    auto rgb = samples<glm::u8vec3>(side + 14, { 80, 120, 160 });
    const unsigned gap = GENERATE(0u, 1u, 128u);
    for (unsigned y = 0; y < side + 14; ++y) {
        for (unsigned x = 0; x < side + 14; ++x) {
            const bool valid = gap == 0 || x % 256 >= gap;
            input.valid.pixel({ x, y }) = valid;
            rgb.valid.pixel({ x, y }) = valid;
        }
    }
    auto fill = nodata::Processor::create(5, 1);
    auto smooth = nodata::Processor::create(0, 5);
    auto combined = nodata::Processor::create(5, 5);
    BENCHMARK("scalar fill only, gap " + std::to_string(gap)) { return fill->process(input, { 7, 7 }, side, {}); };
    BENCHMARK("scalar Gaussian only, gap " + std::to_string(gap)) { return smooth->process(input, { 7, 7 }, side, {}); };
    BENCHMARK("scalar fill and Gaussian, gap " + std::to_string(gap)) { return combined->process(input, { 7, 7 }, side, {}); };
    BENCHMARK("RGB fill and Gaussian, gap " + std::to_string(gap)) { return combined->process(rgb, { 7, 7 }, side, {}); };
}

TEST_CASE("RF NoData full tile memory and parallel benchmark", "[!benchmark][rf-nodata-full]")
{
    initialize_gdal_once();
    const unsigned jobs = GENERATE(1u, 4u);
    auto input = samples<float>(4110, 80);
    for (unsigned y = 0; y < 4110; ++y) {
        for (unsigned x = 0; x < 4110; ++x) {
            input.valid.pixel({ x, y }) = x % 256 >= 128;
        }
    }
    std::vector<nodata::Processor> workers;
    for (unsigned i = 0; i < jobs; ++i) {
        workers.push_back(*nodata::Processor::create(5, 5));
    }
    BENCHMARK("4096px fill and Gaussian, workers " + std::to_string(jobs))
    {
        std::vector<std::future<float>> results;
        for (unsigned i = 0; i < jobs; ++i) {
            results.push_back(std::async(std::launch::async, [&, i] {
                auto output = workers[i].process(input, { 7, 7 }, 4096, {});
                return output ? output->pixel({ 128, 128 }) : -1.f;
            }));
        }
        float value = 0;
        for (auto& result : results) {
            value += result.get();
        }
        return value;
    };
}

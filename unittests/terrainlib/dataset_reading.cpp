/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2022 Adam Celarek-Litofcenko
 * Copyright (C) 2022 Martin Braunsperger
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
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <numbers>
#include <optional>
#include <string>
#include <tuple>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <fmt/core.h>
#include <gdal_priv.h>
#include "../temporary_directory.h"
#include "Dataset.h"
#include "DatasetReader.h"
#include "init.h"
#include "io/image.h"
#include "srs.h"

using namespace radix;

namespace {
// Writes the raster as a grayscale PNG, scaled from its minimum to its maximum.
template <typename T>
Expected<void> debug_out(const radix::Raster<T>& image, const std::filesystem::path& path)
{
    if (image.buffer().empty()) {
        return Error::fail(Error::Code::InvalidInput, "cannot write an empty debug raster", path);
    }
    const auto [min, max] = std::ranges::minmax(image);
    const auto range = float(max) - float(min);
    const auto gray = radix::raster::transform(image, [min, range](const auto value) {
        const auto intensity = range == 0.F ? std::uint8_t(0) : std::uint8_t(255.F * (float(value) - float(min)) / range);
        return glm::u8vec3(intensity);
    });
    return io::image::write(gray, path, { .overwrite = true });
}

void require_projection_available(const Dataset& dataset, const OGRSpatialReference& target_srs)
{
    const auto dataset_srs = dataset.srs().value();
    const auto source_bounds = dataset.bounds().value();

    std::shared_ptr<OGRCoordinateTransformation> transform;
    REQUIRE_NOTHROW(transform = srs::transformation(dataset_srs, target_srs).value());
    REQUIRE(transform != nullptr);

    std::array xs = { (source_bounds.min.x + source_bounds.max.x) / 2.0 };
    std::array ys = { (source_bounds.min.y + source_bounds.max.y) / 2.0 };
    REQUIRE(transform->Transform(static_cast<int>(xs.size()), xs.data(), ys.data()));
}

template <typename Pixel>
DatasetReader<Pixel> make_reader(
    std::shared_ptr<Dataset> dataset, srs::Projection projection, std::array<unsigned, DatasetReader<Pixel>::channel_count> bands, Pixel default_pixel)
{
    auto reader = DatasetReader<Pixel>::make(std::move(dataset), projection, bands, default_pixel);
    INFO((reader ? "" : reader.error().to_string()));
    REQUIRE(reader);
    return std::move(*reader);
}

template <typename Pixel>
typename DatasetReader<Pixel>::Samples read_samples(DatasetReader<Pixel>& reader, const radix::tile::SrsBounds& bounds, glm::uvec2 size)
{
    auto samples = reader.read(bounds, size);
    INFO((samples ? "" : samples.error().to_string()));
    REQUIRE(samples);
    REQUIRE(samples->data.size() == size);
    REQUIRE(samples->valid.size() == size);
    return std::move(*samples);
}

bool all_valid(const radix::Raster<std::uint8_t>& valid)
{
    return std::ranges::all_of(valid, [](std::uint8_t value) { return value != 0; });
}

std::array<double, 6> geo_transform_for(const radix::tile::SrsBounds& bounds, glm::uvec2 size)
{
    return { bounds.min.x, bounds.width() / size.x, 0, bounds.max.y, 0, -bounds.height() / size.y };
}

// Each band is filled with 10 times its number. An empty path creates an
// in-memory raster, otherwise a GeoTIFF.
std::shared_ptr<Dataset> make_raster(
    const std::filesystem::path& path, glm::uvec2 size, unsigned bands, GDALDataType type, std::array<double, 6> geo_transform, int epsg = 3857)
{
    initialize_gdal_once();
    auto* driver = GetGDALDriverManager()->GetDriverByName(path.empty() ? "MEM" : "GTiff");
    REQUIRE(driver != nullptr);
    auto dataset = std::make_shared<Dataset>(driver->Create(path.c_str(), int(size.x), int(size.y), int(bands), type, nullptr));
    const auto reference = srs::from_epsg(epsg).value();
    REQUIRE(dataset->gdalDataset()->SetSpatialRef(&reference) == CE_None);
    REQUIRE(dataset->gdalDataset()->SetGeoTransform(geo_transform.data()) == CE_None);
    for (unsigned band = 1; band <= bands; ++band) {
        REQUIRE(dataset->gdalDataset()->GetRasterBand(int(band))->Fill(10.0 * band) == CE_None);
    }
    return dataset;
}

std::vector<float> generate(glm::uvec2 size, const std::function<float(unsigned, unsigned)>& value)
{
    std::vector<float> values(std::size_t(size.x) * size.y);
    for (unsigned y = 0; y < size.y; ++y) {
        for (unsigned x = 0; x < size.x; ++x) {
            values[std::size_t(y) * size.x + x] = value(x, y);
        }
    }
    return values;
}

void write(GDALRasterBand* band, std::vector<float> values)
{
    const int width = band->GetXSize();
    const int height = band->GetYSize();
    REQUIRE(values.size() == std::size_t(width) * std::size_t(height));
    REQUIRE(band->RasterIO(GF_Write, 0, 0, width, height, values.data(), width, height, GDT_Float32, 0, 0) == CE_None);
}

void write_pixel(GDALRasterBand* band, glm::uvec2 position, float value)
{
    REQUIRE(band->RasterIO(GF_Write, int(position.x), int(position.y), 1, 1, &value, 1, 1, GDT_Float32, 0, 0) == CE_None);
}

class FailingBand final : public GDALRasterBand {
public:
    explicit FailingBand(GDALDataset* owner)
    {
        poDS = owner;
        nBand = 1;
        eDataType = GDT_Float32;
        nBlockXSize = 16;
        nBlockYSize = 1;
    }

protected:
    CPLErr IReadBlock(int, int, void*) override
    {
        CPLError(CE_Failure, CPLE_FileIO, "injected read failure");
        return CE_Failure;
    }
};

class FailingDataset final : public GDALDataset {
public:
    FailingDataset()
        : m_reference(srs::webmercator())
    {
        nRasterXSize = 16;
        nRasterYSize = 16;
        SetBand(1, new FailingBand(this));
    }
    const OGRSpatialReference* GetSpatialRef() const override { return &m_reference; }
    CPLErr GetGeoTransform(double* affine) override
    {
        const std::array<double, 6> values { 0, 1, 0, 16, 0, -1 };
        std::ranges::copy(values, affine);
        return CE_None;
    }

private:
    OGRSpatialReference m_reference;
};
}

TEST_CASE("reading")
{
    const std::vector at100m = {
        "/austria/at_100m_mgi.tif",
        "/austria/at_100m_epsg3857.tif",
        "/austria/at_100m_epsg4326.tif",
        "/austria/at_x149m_y100m_epsg4326.tif",
    };

    const std::vector vienna20m = {
        "/austria/vienna_20m_mgi.tif",
        "/austria/vienna_20m_epsg3857.tif",
        "/austria/vienna_20m_epsg4326.tif",
    };

    const std::vector tauern10m = {
        "/austria/tauern_10m_mgi.tif",
        "/austria/tauern_10m_epsg3857.tif",
        "/austria/tauern_10m_epsg4326.tif",
    };

    const std::vector pizbuin1m = {
        "/austria/pizbuin_1m_mgi.tif",
        "/austria/pizbuin_1m_epsg3857.tif",
        "/austria/pizbuin_1m_epsg4326.tif",
    };

    OGRSpatialReference geographic_srs;
    geographic_srs.importFromEPSG(4326);
    geographic_srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    SECTION("check for min and max heights")
    {
        const std::array test_projections = {
            std::make_pair(srs::Projection::Geographic, 4326),
            std::make_pair(srs::Projection::WebMercator, 3857),
        };
        const std::array test_locations = {
        // CRS bounds, [lower limit, at least one value smaller, at least one value larger, upper limit]
#if defined(ALP_UNITTESTS_EXTENDED) && ALP_UNITTESTS_EXTENDED
            std::make_tuple(
                "at100m",
                at100m,
                radix::tile::SrsBounds{{9.5, 46.4}, {17.1, 49.0}},
                std::make_tuple(100.0f, 120.0f, 3000.0f, 3800.0f)), // Austria is between 115 and 3798m
            std::make_tuple(
                "vienna20m",
                vienna20m,
                radix::tile::SrsBounds{{16.17, 48.14}, {16.59, 48.33}},
                std::make_tuple(140.0f, 180.0f, 500.0f, 560.0f)), // vienna, between 151 and 542m
#endif
            std::make_tuple(
                "tauern10m",
                tauern10m,
                radix::tile::SrsBounds{{12.6934117, 47.0739300}, {12.6944580, 47.0748649}},
                std::make_tuple(3700.0f, 3720.0f, 3790.0f, 3800.0f)), // Grossglockner, 3798m with some surroundings
        };

        for (const auto& test : test_locations) {
            const auto [test_name, test_datasets, geographic_bounds, limits] = test;

            for (std::string dataset_name : test_datasets) {
                const auto dataset = Error::throwing_unwrap(Dataset::open_shared_raster(ALP_TEST_DATA_DIR + std::string(dataset_name)));
                for (const auto& [test_projection, test_srs] : test_projections) {
                    OGRSpatialReference srs;
                    srs.importFromEPSG(test_srs);
                    srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

                    const auto srs_corners = srs::transform_points(geographic_srs, srs, std::array { geographic_bounds.min, geographic_bounds.max }).value();
                    const radix::tile::SrsBounds srs_bounds { srs_corners[0], srs_corners[1] };

                    require_projection_available(*dataset, srs);
                    auto reader = make_reader(dataset, test_projection, { 1 }, 0.0F);
                    REQUIRE(reader.projection() == test_projection);
                    if (ALP_UNITTESTS_DEBUG_IMAGES) {
                        const auto debug_samples = read_samples(reader, srs_bounds, { 1000, 1000 });
                        const auto s = std::string("/austria/").length();
                        const auto l = dataset_name.length() - std::string(".tif").length() - s;
                        const auto debug_file = fmt::format("./heights_{}_srs{}_{}.png", test_name, test_srs, dataset_name.substr(s, l));
                        REQUIRE(debug_out(debug_samples.data, debug_file).has_value());
                    }

                    const auto samples = read_samples(reader, srs_bounds, { 100, 111 });
                    CHECK(all_valid(samples.valid));
                    const auto& heights = samples.data;
                    REQUIRE(heights.width() == 100);
                    REQUIRE(heights.height() == 111);
                    const auto [lower_bound, lower_than, higher_than, higher_bound] = limits;
                    auto [min, max] = std::ranges::minmax(heights);
                    CHECK(min > lower_bound);
                    CHECK(min < lower_than);
                    CHECK(max < higher_bound);
                    CHECK(max > higher_than);
                }
            }
        }
    }

    SECTION("compare with ref render")
    {
        const std::array test_data = {
#if defined(ALP_UNITTESTS_EXTENDED) && ALP_UNITTESTS_EXTENDED
            std::make_tuple(
                "at100m",
                at100m,
                radix::tile::SrsBounds{{9.5, 46.4}, {17.1, 49.0}},
                620U, 350U, 42.0, 22.0),
#endif
            std::make_tuple(
                "pizbuin1m",
                pizbuin1m,
                // Both reads use the approximate transformer; exact transforms stay below 2.7.
                radix::tile::SrsBounds{{10.105646780, 46.839864531}, {10.129815588, 46.847626067}},
                740U, 315U, 3.2, 0.006),
#if defined(ALP_UNITTESTS_EXTENDED) && ALP_UNITTESTS_EXTENDED
            std::make_tuple(
                "pizbuin1m_highres",
                pizbuin1m,
                radix::tile::SrsBounds{{10.105646780, 46.839864531}, {10.129815588, 46.847626067}},
                2000U, 850U, 10.0, 0.008),
#endif
        };

        for (const auto& test : test_data) {
            auto [test_name, datasets, ref_bounds, render_width, render_height, max_abs_diff, max_mse] = test;

            const auto ref_dataset = Error::throwing_unwrap(Dataset::open_shared_raster(ALP_TEST_DATA_DIR + std::string(datasets.front())));
            require_projection_available(*ref_dataset, geographic_srs);
            auto ref_reader = make_reader(ref_dataset, srs::Projection::Geographic, { 1 }, 0.0F);
            const auto ref_samples = read_samples(ref_reader, ref_bounds, { render_width, render_height });
            CHECK(all_valid(ref_samples.valid));
            const auto& ref_heights = ref_samples.data;
            if (ALP_UNITTESTS_DEBUG_IMAGES) {
                REQUIRE(debug_out(ref_heights, fmt::format("./heights_ref.png")).has_value());
            }

            for (std::string dataset_name : datasets) {
                const auto dataset = Error::throwing_unwrap(Dataset::open_shared_raster(ALP_TEST_DATA_DIR + std::string(dataset_name)));
                require_projection_available(*dataset, geographic_srs);
                auto reader = make_reader(dataset, srs::Projection::Geographic, { 1 }, 0.0F);
                const auto samples = read_samples(reader, ref_bounds, { render_width, render_height });
                CHECK(all_valid(samples.valid));
                const auto& heights = samples.data;

                const auto s = std::string("/austria/").length();
                const auto l = dataset_name.length() - std::string(".tif").length() - s;

                if (ALP_UNITTESTS_DEBUG_IMAGES) {
                    REQUIRE(debug_out(ref_heights, fmt::format("./heights_{}_{}.png", test_name, dataset_name.substr(s, l))).has_value());

                    auto height_diffs = radix::Raster<float>({ render_width, render_height });
                    std::transform(ref_heights.begin(), ref_heights.end(), heights.begin(), height_diffs.begin(), [](auto a, auto b) { return std::abs(a - b); });
                    const auto path = fmt::format("./diffs_{}_{}.png", test_name, dataset_name.substr(s, l));
                    REQUIRE(debug_out(height_diffs, path).has_value());
                }
                auto largest_abs_diff = 0.0;
                const auto mse = std::transform_reduce(ref_heights.begin(), ref_heights.end(), heights.begin(), 0.0, std::plus<>(), [&largest_abs_diff](auto a, auto b) {
                    const auto t = std::abs(double(a) - double(b));
                    largest_abs_diff = std::max(t, largest_abs_diff);
                    return t * t;
                }) / double(ref_heights.buffer_length());
                //        fmt::print("{} | {};  mse: {}, largest_abs_diff: {}\n", test_name, dataset_name.substr(s, l), mse, largest_abs_diff);
                CHECK(largest_abs_diff < double(max_abs_diff));
                CHECK(mse < max_mse);
            }
        }
    }
}

TEST_CASE("min pixel size")
{
    using Projection = srs::Projection;
    const auto open = [](const char* name) { return Error::throwing_unwrap(Dataset::open_shared_raster(ALP_TEST_DATA_DIR + std::string(name))); };
    const auto reader = [](const std::shared_ptr<Dataset>& dataset, Projection projection) { return make_reader(dataset, projection, { 1 }, 0.0F); };
    const auto pixel_size = [](const std::shared_ptr<Dataset>& dataset) {
        std::array<double, 6> geo_transform {};
        REQUIRE(dataset->gdalDataset()->GetGeoTransform(geo_transform.data()) == CE_None);
        return glm::dvec2(geo_transform[1], -geo_transform[5]);
    };
    const double metres_per_degree = 6378137.0 * std::numbers::pi / 180;
    // Within all Austrian test datasets.
    const radix::tile::SrsBounds geographic_bounds { { 12.0, 47.0 }, { 14.0, 48.0 } };
    const auto mercator_corners = srs::transform_points(srs::wgs84(), srs::webmercator(), std::array { geographic_bounds.min, geographic_bounds.max }).value();
    const radix::tile::SrsBounds mercator_bounds { mercator_corners[0], mercator_corners[1] };

    SECTION("same srs as the dataset")
    {
        const auto mercator = open("/austria/at_100m_epsg3857.tif");
        const auto mercator_size = reader(mercator, Projection::WebMercator).min_pixel_size(mercator_bounds);
        REQUIRE(mercator_size);
        CHECK(mercator_size->x == Catch::Approx(pixel_size(mercator).x));
        CHECK(mercator_size->y == Catch::Approx(pixel_size(mercator).y));

        const auto anisotropic = open("/austria/at_x149m_y100m_epsg4326.tif");
        const auto geographic_size = reader(anisotropic, Projection::Geographic).min_pixel_size(geographic_bounds);
        REQUIRE(geographic_size);
        CHECK(geographic_size->x == Catch::Approx(pixel_size(anisotropic).x));
        CHECK(geographic_size->y == Catch::Approx(pixel_size(anisotropic).y));
    }

    SECTION("geographic dataset in web mercator")
    {
        // Web Mercator stretches y by 1 / cos(latitude), the minimum is at the southern edge.
        const auto dataset = open("/austria/at_100m_epsg4326.tif");
        const auto size = reader(dataset, Projection::WebMercator).min_pixel_size(mercator_bounds);
        REQUIRE(size);
        const double south = geographic_bounds.min.y * std::numbers::pi / 180;
        CHECK(size->x == Catch::Approx(pixel_size(dataset).x * metres_per_degree).epsilon(1e-3));
        CHECK(size->y == Catch::Approx(pixel_size(dataset).y * metres_per_degree / std::cos(south)).epsilon(1e-3));
    }

    SECTION("bounds larger than the dataset are clipped to its coverage")
    {
        const auto dataset = open("/austria/at_100m_epsg4326.tif");
        const radix::tile::SrsBounds world { glm::dvec2(-srs::webmercator_half_extent), glm::dvec2(srs::webmercator_half_extent) };
        const auto size = reader(dataset, Projection::WebMercator).min_pixel_size(world);
        REQUIRE(size);
        const double south = dataset->bounds().value().min.y * std::numbers::pi / 180;
        CHECK(size->y == Catch::Approx(pixel_size(dataset).y * metres_per_degree / std::cos(south)).epsilon(1e-3));
    }

    SECTION("projected dataset in web mercator")
    {
        // About 100 m on the ground, i.e. 1 / cos(47°) * 100 m in Web Mercator, reduced slightly by the grid rotation.
        const auto dataset = open("/austria/at_100m_mgi.tif");
        const auto size = reader(dataset, Projection::WebMercator).min_pixel_size(mercator_bounds);
        REQUIRE(size);
        CHECK(size->x > 130);
        CHECK(size->x < 155);
        CHECK(size->y > 130);
        CHECK(size->y < 155);
    }

    SECTION("bounds outside the coverage")
    {
        const auto dataset = open("/austria/at_100m_epsg4326.tif");
        CHECK(!reader(dataset, Projection::Geographic).min_pixel_size({ { 0.0, 0.0 }, { 1.0, 1.0 } }));
    }
}

TEST_CASE("geographic datasets wider than 360 degrees wrap around their centre")
{
    initialize_gdal_once();
    // 0.5° pixels from -0.5 to 360.5, each storing its centre longitude. GDAL only wraps
    // sources overlapping themselves by at most about one pixel.
    constexpr int width = 722;
    constexpr int height = 240;
    auto* driver = GetGDALDriverManager()->GetDriverByName("MEM");
    REQUIRE(driver != nullptr);
    const auto dataset = std::make_shared<Dataset>(driver->Create("", width, height, 1, GDT_Float32, nullptr));
    std::array<double, 6> geo_transform { -0.5, 0.5, 0, 60, 0, -0.5 };
    REQUIRE(dataset->gdalDataset()->SetGeoTransform(geo_transform.data()) == CE_None);
    const auto reference = srs::wgs84();
    REQUIRE(dataset->gdalDataset()->SetSpatialRef(&reference) == CE_None);
    std::vector<float> longitudes(std::size_t(width) * height);
    for (std::size_t i = 0; i < longitudes.size(); ++i) {
        longitudes[i] = float(-0.25 + 0.5 * double(i % width));
    }
    REQUIRE(dataset->gdalDataset()->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, width, height, longitudes.data(), width, height, GDT_Float32, 0, 0) == CE_None);

    auto reader = make_reader(dataset, srs::Projection::WebMercator, { 1 }, 0.0F);
    const double degree = srs::webmercator_half_extent / 180;
    for (const double longitude : { -90.0, 90.0 }) {
        const auto samples = read_samples(reader, { { (longitude - 1) * degree, -degree }, { (longitude + 1) * degree, degree } }, { 4, 4 });
        CHECK(all_valid(samples.valid));
        for (const float value : samples.data.buffer()) {
            CHECK(value == Catch::Approx(longitude < 0 ? longitude + 360 : longitude).margin(1.5));
        }
    }
}

TEST_CASE("dataset reader reads the selected bands in order", "[dataset_reader]")
{
    const auto bounds = srs::webmercator_tile_bounds({ 5, { 16, 15 } });
    const auto geo_transform = geo_transform_for(bounds, glm::uvec2(32));
    SECTION("scalar")
    {
        auto reader = make_reader(make_raster({}, glm::uvec2(32), 3, GDT_Float32, geo_transform), srs::Projection::WebMercator, { 2 }, 0.0F);
        for (const auto size : { glm::uvec2(32), glm::uvec2(24, 40) }) {
            const auto samples = read_samples(reader, bounds, size);
            CHECK(all_valid(samples.valid));
            CHECK(std::ranges::all_of(samples.data, [](float value) { return value == Catch::Approx(20); }));
        }
    }
    SECTION("RGB")
    {
        auto reader = make_reader(make_raster({}, glm::uvec2(32), 4, GDT_Byte, geo_transform), srs::Projection::WebMercator, { 3, 1, 4 }, glm::u8vec3(0));
        for (const auto size : { glm::uvec2(32), glm::uvec2(24, 40) }) {
            const auto samples = read_samples(reader, bounds, size);
            CHECK(all_valid(samples.valid));
            CHECK(std::ranges::all_of(samples.data, [](glm::u8vec3 value) { return value == glm::u8vec3(30, 10, 40); }));
        }
    }
}

TEST_CASE("dataset reader rejects unsupported sources and invalid requests", "[dataset_reader]")
{
    const auto bounds = srs::webmercator_tile_bounds({ 5, { 16, 15 } });
    const auto geo_transform = geo_transform_for(bounds, glm::uvec2(16));
    const auto raster = [&](GDALDataType type) { return make_raster({}, glm::uvec2(16), 3, type, geo_transform); };
    const auto require_code = [](const auto& result, Error::Code code) {
        REQUIRE_FALSE(result);
        CHECK(result.error().code() == code);
    };
    using Projection = srs::Projection;

    // No source type conversion.
    for (const auto type : { GDT_Byte, GDT_Int16, GDT_Float64 }) {
        require_code(DatasetReader<float>::make(raster(type), Projection::WebMercator, { 1 }, 0.0F), Error::Code::Unsupported);
    }
    for (const auto type : { GDT_Float32, GDT_UInt16 }) {
        require_code(DatasetReader<glm::u8vec3>::make(raster(type), Projection::WebMercator, { 1, 2, 3 }, glm::u8vec3(0)), Error::Code::Unsupported);
    }
    for (const unsigned band : { 0u, 4u }) {
        require_code(DatasetReader<float>::make(raster(GDT_Float32), Projection::WebMercator, { band }, 0.0F), Error::Code::InvalidInput);
        require_code(DatasetReader<glm::u8vec3>::make(raster(GDT_Byte), Projection::WebMercator, { 1, band, 3 }, glm::u8vec3(0)), Error::Code::InvalidInput);
    }
    for (const float default_pixel : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() }) {
        require_code(DatasetReader<float>::make(raster(GDT_Float32), Projection::WebMercator, { 1 }, default_pixel), Error::Code::InvalidInput);
    }
    auto unreferenced = raster(GDT_Float32);
    REQUIRE(unreferenced->gdalDataset()->SetSpatialRef(nullptr) == CE_None);
    require_code(DatasetReader<float>::make(unreferenced, Projection::WebMercator, { 1 }, 0.0F), Error::Code::InvalidInput);

    // Sizes and bounds are validated before allocation and narrowing to GDAL's int.
    auto reader = make_reader(raster(GDT_Float32), Projection::WebMercator, { 1 }, 0.0F);
    const unsigned beyond_int = unsigned(INT_MAX) + 1;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    const std::array<std::tuple<radix::tile::SrsBounds, glm::uvec2, Error::Code>, 10> requests { {
        { bounds, { 0, 16 }, Error::Code::InvalidInput },
        { bounds, { 16, 0 }, Error::Code::InvalidInput },
        { bounds, { beyond_int, 1 }, Error::Code::InvalidInput },
        { bounds, { 1, beyond_int }, Error::Code::InvalidInput },
        { bounds, { INT_MAX, INT_MAX }, Error::Code::ResourceExhausted },
        { { bounds.min, bounds.min }, { 16, 16 }, Error::Code::InvalidInput },
        { { bounds.max, bounds.min }, { 16, 16 }, Error::Code::InvalidInput },
        { { { nan, 0 }, { 1, 1 } }, { 16, 16 }, Error::Code::InvalidInput },
        { { { -infinity, 0 }, { 1, 1 } }, { 16, 16 }, Error::Code::InvalidInput },
        { { { -DBL_MAX, 0 }, { DBL_MAX, 1 } }, { 16, 16 }, Error::Code::InvalidInput },
    } };
    for (const auto& [request_bounds, size, code] : requests) {
        require_code(reader.read(request_bounds, size), code);
    }
    auto rgb = make_reader(raster(GDT_Byte), Projection::WebMercator, { 1, 2, 3 }, glm::u8vec3(0));
    require_code(rgb.read(bounds, { INT_MAX, INT_MAX }), Error::Code::ResourceExhausted);
}

TEST_CASE("dataset reader returns GDAL read failures", "[dataset_reader]")
{
    initialize_gdal_once();
    auto reader = make_reader(std::make_shared<Dataset>(new FailingDataset()), srs::Projection::WebMercator, { 1 }, 0.0F);
    const auto samples = reader.read({ { 0, 0 }, { 16, 16 } }, { 8, 8 });
    REQUIRE_FALSE(samples);
    CHECK(samples.error().code() == Error::Code::Io);
}

TEST_CASE("dataset reader combines the effective source masks", "[dataset_reader]")
{
    const auto bounds = srs::webmercator_tile_bounds({ 5, { 16, 15 } });
    const glm::uvec2 size(32);
    const glm::u8vec3 fallback(1, 2, 3);
    auto dataset = make_raster({}, size, 4, GDT_Byte, geo_transform_for(bounds, size));
    auto* gdal_dataset = dataset->gdalDataset();
    using Region = std::function<bool(unsigned, unsigned)>;
    const Region left = [](unsigned x, unsigned) { return x < 16; };
    const Region top = [](unsigned, unsigned y) { return y < 16; };
    const auto values = [&](const Region& valid) { return generate(size, [&](unsigned x, unsigned y) { return valid(x, y) ? 255.f : 0.f; }); };
    const auto mask = [&](int band, int flags, const Region& valid) {
        REQUIRE(gdal_dataset->GetRasterBand(band)->CreateMaskBand(flags) == CE_None);
        write(gdal_dataset->GetRasterBand(band)->GetMaskBand(), values(valid));
    };
    Region expected = [](unsigned, unsigned) { return true; };
    SECTION("all valid") { }
    SECTION("band NoData")
    {
        REQUIRE(gdal_dataset->GetRasterBand(2)->SetNoDataValue(20) == CE_None);
        expected = [](unsigned, unsigned) { return false; };
    }
    SECTION("explicit mask of one band")
    {
        mask(2, 0, left);
        expected = left;
    }
    SECTION("shared per-dataset mask")
    {
        mask(1, GMF_PER_DATASET, top);
        REQUIRE(gdal_dataset->GetRasterBand(3)->GetMaskBand() == gdal_dataset->GetRasterBand(1)->GetMaskBand());
        expected = top;
    }
    SECTION("alpha band")
    {
        REQUIRE(gdal_dataset->GetRasterBand(4)->SetColorInterpretation(GCI_AlphaBand) == CE_None);
        write(gdal_dataset->GetRasterBand(4), values(top));
        REQUIRE((gdal_dataset->GetRasterBand(1)->GetMaskFlags() & GMF_ALPHA) != 0);
        expected = top;
    }
    SECTION("different masks per band")
    {
        mask(1, 0, left);
        mask(3, 0, top);
        expected = [&](unsigned x, unsigned y) { return left(x, y) && top(x, y); };
    }
    auto reader = make_reader(dataset, srs::Projection::WebMercator, { 1, 2, 3 }, fallback);
    const auto samples = read_samples(reader, bounds, size);
    for (unsigned y = 0; y < size.y; ++y) {
        for (unsigned x = 0; x < size.x; ++x) {
            INFO(fmt::format("pixel {}, {}", x, y));
            CHECK((samples.valid.pixel({ x, y }) != 0) == expected(x, y));
            CHECK(samples.data.pixel({ x, y }) == (expected(x, y) ? glm::u8vec3(10, 20, 30) : fallback));
        }
    }
}

TEST_CASE("dataset reader prefers an explicit mask over a conflicting NoData value", "[dataset_reader]")
{
    const auto bounds = srs::webmercator_tile_bounds({ 5, { 16, 15 } });
    auto dataset = make_raster({}, glm::uvec2(32), 1, GDT_Float32, geo_transform_for(bounds, glm::uvec2(32)));
    auto* band = dataset->gdalDataset()->GetRasterBand(1);
    REQUIRE(band->SetNoDataValue(10) == CE_None);
    REQUIRE(band->CreateMaskBand(0) == CE_None);
    REQUIRE(band->GetMaskBand()->Fill(255) == CE_None);
    auto reader = make_reader(dataset, srs::Projection::WebMercator, { 1 }, 0.0F);
    const auto samples = read_samples(reader, bounds, glm::uvec2(24));
    CHECK(all_valid(samples.valid));
    CHECK(std::ranges::all_of(samples.data, [](float value) { return value == Catch::Approx(10); }));
}

TEST_CASE("dataset reader keeps valid pixels equal to sentinels or the default", "[dataset_reader]")
{
    const auto bounds = srs::webmercator_tile_bounds({ 5, { 16, 15 } });
    const auto geo_transform = geo_transform_for(bounds, glm::uvec2(32));
    SECTION("black RGB with a black default")
    {
        auto dataset = make_raster({}, glm::uvec2(32), 3, GDT_Byte, geo_transform);
        for (int band = 1; band <= 3; ++band) {
            REQUIRE(dataset->gdalDataset()->GetRasterBand(band)->Fill(0) == CE_None);
        }
        auto reader = make_reader(dataset, srs::Projection::WebMercator, { 1, 2, 3 }, glm::u8vec3(0));
        const auto samples = read_samples(reader, bounds, glm::uvec2(32));
        CHECK(all_valid(samples.valid));
        CHECK(std::ranges::all_of(samples.data, [](glm::u8vec3 value) { return value == glm::u8vec3(0); }));
    }
    SECTION("scalar equal to the default")
    {
        auto reader = make_reader(make_raster({}, glm::uvec2(32), 1, GDT_Float32, geo_transform), srs::Projection::WebMercator, { 1 }, 10.0F);
        const auto samples = read_samples(reader, bounds, glm::uvec2(32));
        CHECK(all_valid(samples.valid));
        CHECK(std::ranges::all_of(samples.data, [](float value) { return value == 10; }));
    }
    SECTION("scalar -32768 without NoData")
    {
        auto dataset = make_raster({}, glm::uvec2(32), 1, GDT_Float32, geo_transform);
        REQUIRE(dataset->gdalDataset()->GetRasterBand(1)->Fill(-32768) == CE_None);
        auto reader = make_reader(dataset, srs::Projection::WebMercator, { 1 }, 0.0F);
        const auto samples = read_samples(reader, bounds, glm::uvec2(32));
        CHECK(all_valid(samples.valid));
        CHECK(std::ranges::all_of(samples.data, [](float value) { return value == -32768; }));
    }
}

namespace {
template <typename Pixel>
void check_coverage(GDALDataType type, std::array<unsigned, DatasetReader<Pixel>::channel_count> bands, Pixel inside, Pixel fallback)
{
    const auto bounds = srs::webmercator_tile_bounds({ 5, { 16, 15 } });
    const glm::uvec2 size(32);
    // The source covers the western half of the output.
    auto dataset = make_raster({}, { 16, 32 }, unsigned(bands.size()), type, geo_transform_for(bounds, size));
    auto reader = make_reader(dataset, srs::Projection::WebMercator, bands, fallback);
    const auto partial = read_samples(reader, bounds, size);
    for (unsigned y = 0; y < size.y; ++y) {
        for (unsigned x = 0; x < size.x; ++x) {
            INFO(fmt::format("pixel {}, {}", x, y));
            CHECK((partial.valid.pixel({ x, y }) != 0) == (x < 16));
            CHECK(partial.data.pixel({ x, y }) == (x < 16 ? inside : fallback));
        }
    }
    const auto absent = read_samples(reader, srs::webmercator_tile_bounds({ 5, { 2, 2 } }), size);
    CHECK(std::ranges::none_of(absent.valid, [](std::uint8_t value) { return value != 0; }));
    CHECK(std::ranges::all_of(absent.data, [&](Pixel value) { return value == fallback; }));
}
} // namespace

TEST_CASE("dataset reader returns defaults outside the source coverage", "[dataset_reader]")
{
    SECTION("scalar") { check_coverage<float>(GDT_Float32, { 1 }, 10.0F, 7.5F); }
    SECTION("RGB") { check_coverage<glm::u8vec3>(GDT_Byte, { 1, 2, 3 }, glm::u8vec3(10, 20, 30), glm::u8vec3(1, 2, 3)); }
}

TEST_CASE("dataset reader replaces nonfinite float results with the default", "[dataset_reader]")
{
    const float nonfinite = GENERATE(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());
    const auto bounds = srs::webmercator_tile_bounds({ 5, { 16, 15 } });
    auto dataset = make_raster({}, glm::uvec2(32), 1, GDT_Float32, geo_transform_for(bounds, glm::uvec2(32)));
    auto* band = dataset->gdalDataset()->GetRasterBand(1);
    auto reader = make_reader(dataset, srs::Projection::WebMercator, { 1 }, 7.5F);
    SECTION("everywhere")
    {
        REQUIRE(band->Fill(nonfinite) == CE_None);
        for (const auto size : { glm::uvec2(32), glm::uvec2(24) }) {
            const auto samples = read_samples(reader, bounds, size);
            CHECK(std::ranges::none_of(samples.valid, [](std::uint8_t value) { return value != 0; }));
            CHECK(std::ranges::all_of(samples.data, [](float value) { return value == 7.5F; }));
        }
    }
    SECTION("one unmasked sample contaminates its filter support")
    {
        write_pixel(band, { 16, 16 }, nonfinite);
        const auto samples = read_samples(reader, bounds, glm::uvec2(24));
        for (std::size_t i = 0; i < samples.data.buffer().size(); ++i) {
            if (samples.valid.buffer()[i]) {
                CHECK(samples.data.buffer()[i] == Catch::Approx(10));
            } else {
                CHECK(samples.data.buffer()[i] == 7.5F);
            }
        }
        // Centred on the nonfinite sample, and far from it.
        CHECK(samples.valid.pixel({ 12, 12 }) == 0);
        CHECK(samples.valid.pixel({ 0, 0 }) != 0);
    }
}

TEST_CASE("dataset reader excludes a sample invalid in one channel from all channels", "[dataset_reader]")
{
    const auto bounds = srs::webmercator_tile_bounds({ 5, { 16, 15 } });
    auto dataset = make_raster({}, glm::uvec2(32), 3, GDT_Byte, geo_transform_for(bounds, glm::uvec2(32)));
    auto* gdal_dataset = dataset->gdalDataset();
    for (int band = 1; band <= 3; ++band) {
        REQUIRE(gdal_dataset->GetRasterBand(band)->Fill(100) == CE_None);
    }
    // A red outlier, invalid only through the green NoData.
    write_pixel(gdal_dataset->GetRasterBand(1), { 16, 16 }, 250);
    write_pixel(gdal_dataset->GetRasterBand(2), { 16, 16 }, 0);
    REQUIRE(gdal_dataset->GetRasterBand(2)->SetNoDataValue(0) == CE_None);
    const glm::u8vec3 fallback(1, 2, 3);
    auto reader = make_reader(dataset, srs::Projection::WebMercator, { 1, 2, 3 }, fallback);
    // GDAL does not write outputs centred on an invalid sample. The Lanczos
    // supports of the others contain valid samples. Warping each channel with
    // its own mask, red would include the outlier around it.
    const auto samples = read_samples(reader, bounds, glm::uvec2(24));
    for (unsigned y = 0; y < 24; ++y) {
        for (unsigned x = 0; x < 24; ++x) {
            INFO(fmt::format("pixel {}, {}", x, y));
            const bool on_outlier = x == 12 && y == 12;
            CHECK((samples.valid.pixel({ x, y }) != 0) == !on_outlier);
            CHECK(samples.data.pixel({ x, y }) == (on_outlier ? fallback : glm::u8vec3(100)));
        }
    }
}

TEST_CASE("dataset reader reprojects within the approximation tolerance", "[dataset_reader]")
{
    // 0.1° pixels storing the longitude and latitude of their centres.
    constexpr unsigned side = 200;
    auto dataset = make_raster({}, glm::uvec2(side), 2, GDT_Float32, { 0, 0.1, 0, 20, 0, -0.1 }, 4326);
    write(dataset->gdalDataset()->GetRasterBand(1), generate(glm::uvec2(side), [](unsigned x, unsigned) { return float((x + 0.5) * 0.1); }));
    write(dataset->gdalDataset()->GetRasterBand(2), generate(glm::uvec2(side), [](unsigned, unsigned y) { return float(20 - (y + 0.5) * 0.1); }));
    const auto corners = srs::transform_points(srs::wgs84(), srs::webmercator(), std::array { glm::dvec2(5, 5), glm::dvec2(15, 15) }).value();
    const radix::tile::SrsBounds bounds { corners[0], corners[1] };
    const glm::uvec2 size(64, 80);
    const double radius = 6378137;
    for (const unsigned band : { 1u, 2u }) {
        auto reader = make_reader(dataset, srs::Projection::WebMercator, { band }, 0.0F);
        const auto samples = read_samples(reader, bounds, size);
        CHECK(all_valid(samples.valid));
        for (unsigned y = 0; y < size.y; ++y) {
            for (unsigned x = 0; x < size.x; ++x) {
                const double mercator_x = bounds.min.x + (x + 0.5) * bounds.width() / size.x;
                const double mercator_y = bounds.max.y - (y + 0.5) * bounds.height() / size.y;
                const double expected = band == 1 ? mercator_x / radius * 180 / std::numbers::pi
                                                  : (2 * std::atan(std::exp(mercator_y / radius)) - std::numbers::pi / 2) * 180 / std::numbers::pi;
                // Within 0.2 source pixels.
                CHECK(samples.data.pixel({ x, y }) == Catch::Approx(expected).margin(0.02));
            }
        }
    }
}

TEST_CASE("dataset reader reads a VRT mosaic like a single raster at base resolution", "[dataset_reader]")
{
    test::TemporaryDirectory directory("dataset-reader");
    {
        make_raster(directory.path() / "left.tif", glm::uvec2(16), 1, GDT_Float32, { 0, 1, 0, 16, 0, -1 });
        auto right = make_raster(directory.path() / "right.tif", glm::uvec2(16), 1, GDT_Float32, { 16, 1, 0, 16, 0, -1 });
        REQUIRE(right->gdalDataset()->GetRasterBand(1)->Fill(50) == CE_None);
    }
    const auto vrt_path = directory.path() / "mosaic.vrt";
    {
        std::ofstream vrt(vrt_path);
        vrt << fmt::format(R"(<VRTDataset rasterXSize="32" rasterYSize="16">
<SRS>EPSG:3857</SRS><GeoTransform>0,1,0,16,0,-1</GeoTransform>
<VRTRasterBand dataType="Float32" band="1">
<SimpleSource><SourceFilename>{}</SourceFilename><SourceBand>1</SourceBand><SrcRect xOff="0" yOff="0" xSize="16" ySize="16"/><DstRect xOff="0" yOff="0" xSize="16" ySize="16"/></SimpleSource>
<SimpleSource><SourceFilename>{}</SourceFilename><SourceBand>1</SourceBand><SrcRect xOff="0" yOff="0" xSize="16" ySize="16"/><DstRect xOff="16" yOff="0" xSize="16" ySize="16"/></SimpleSource>
</VRTRasterBand></VRTDataset>)",
            (directory.path() / "left.tif").string(),
            (directory.path() / "right.tif").string());
        REQUIRE(vrt.good());
    }
    auto mosaic = make_reader(Error::throwing_unwrap(Dataset::open_shared_raster(vrt_path)), srs::Projection::WebMercator, { 1 }, 0.0F);
    const radix::tile::SrsBounds window { { 8.25, 0.25 }, { 24.25, 16.25 } };
    const auto combined = read_samples(mosaic, window, glm::uvec2(16));
    auto reference = make_raster({}, glm::uvec2(32), 1, GDT_Float32, { 0, 1, 0, 16, 0, -1 });
    write(reference->gdalDataset()->GetRasterBand(1), generate(glm::uvec2(32), [](unsigned x, unsigned) { return x < 16 ? 10.f : 50.f; }));
    auto single = make_reader(reference, srs::Projection::WebMercator, { 1 }, 0.0F);
    const auto expected = read_samples(single, window, glm::uvec2(16));
    // Compare the source assembly, not GDAL's filter formula.
    for (unsigned x = 0; x < 16; ++x) {
        CHECK(combined.valid.pixel({ x, 8 }) != 0);
        CHECK(combined.data.pixel({ x, 8 }) == Catch::Approx(expected.data.pixel({ x, 8 })));
    }
    const int level = 2;
    REQUIRE(reference->gdalDataset()->BuildOverviews("NEAREST", 1, &level, 0, nullptr, nullptr, nullptr) == CE_None);
    REQUIRE(reference->gdalDataset()->GetRasterBand(1)->GetOverview(0)->Fill(222) == CE_None);
    const auto base = read_samples(single, { { 0, -16 }, { 32, 16 } }, glm::uvec2(16));
    CHECK(base.valid.pixel({ 4, 8 }) != 0);
    CHECK(base.data.pixel({ 4, 8 }) != 222);
}

TEST_CASE("dataset reader agrees across adjacent output windows", "[dataset_reader]")
{
    // 0.25° pixels storing the sum of their column and row, away from the antimeridian.
    auto dataset = make_raster({}, glm::uvec2(64), 1, GDT_Float32, { 0, 0.25, 0, 8, 0, -0.25 }, 4326);
    write(dataset->gdalDataset()->GetRasterBand(1), generate(glm::uvec2(64), [](unsigned x, unsigned y) { return float(x + y); }));
    auto reader = make_reader(dataset, srs::Projection::WebMercator, { 1 }, 0.0F);
    const double width = 2 * srs::webmercator_half_extent / 180;
    const glm::dvec2 centre(4 * width, 0);
    const auto west = read_samples(reader, { centre - glm::dvec2(width, width / 2), centre + glm::dvec2(0, width / 2) }, glm::uvec2(16));
    const auto east = read_samples(reader, { centre - glm::dvec2(0, width / 2), centre + glm::dvec2(width, width / 2) }, glm::uvec2(16));
    const auto joined = read_samples(reader, { centre - glm::dvec2(width), centre + glm::dvec2(width) }, glm::uvec2(32));
    CHECK(all_valid(west.valid));
    CHECK(all_valid(east.valid));
    CHECK(all_valid(joined.valid));
    // Each window may deviate by the approximation error of 0.125 source pixels per axis.
    for (unsigned row = 0; row < 16; ++row) {
        for (unsigned column = 0; column < 16; ++column) {
            CHECK(west.data.pixel({ column, row }) == Catch::Approx(joined.data.pixel({ column, row + 8 })).margin(0.5));
            CHECK(east.data.pixel({ column, row }) == Catch::Approx(joined.data.pixel({ column + 16, row + 8 })).margin(0.5));
        }
    }
}

TEST_CASE("dataset reader ignores the GDAL threading environment", "[dataset_reader]")
{
    struct Configuration {
        const char* name;
        std::optional<std::string> previous = std::nullopt;
        Configuration(const char* option, const char* value)
            : name(option)
        {
            if (const auto* old = CPLGetConfigOption(name, nullptr)) {
                previous = old;
            }
            CPLSetConfigOption(name, value);
        }
        ~Configuration() { CPLSetConfigOption(name, previous ? previous->c_str() : nullptr); }
    };
    const Configuration threads("GDAL_NUM_THREADS", "2");
    const Configuration chunks("WARP_THREAD_CHUNK_SIZE", "1");
    const auto bounds = srs::webmercator_tile_bounds({ 5, { 16, 15 } });
    auto reader
        = make_reader(make_raster({}, glm::uvec2(32), 1, GDT_Float32, geo_transform_for(bounds, glm::uvec2(32))), srs::Projection::WebMercator, { 1 }, 0.0F);
    const auto samples = read_samples(reader, bounds, glm::uvec2(24));
    CHECK(CPLGetLastErrorType() < CE_Failure);
    CHECK(all_valid(samples.valid));
}

TEST_CASE("dataset reader finds a coarse projected source in a world tile", "[dataset_reader]")
{
    // Many probes of the world tile fail to transform into UTM.
    auto dataset = make_raster({}, glm::uvec2(4), 1, GDT_Float32, { -3500000, 2000000, 0, 4000000, 0, -2000000 }, 32632);
    auto reader = make_reader(dataset, srs::Projection::WebMercator, { 1 }, 0.0F);
    const auto samples = read_samples(reader, srs::webmercator_tile_bounds({ 0, { 0, 0 } }), glm::uvec2(16));
    // Around 11° east and north, inside the footprint.
    CHECK(samples.valid.pixel({ 8, 7 }) != 0);
    CHECK(samples.data.pixel({ 8, 7 }) == Catch::Approx(10));
}

TEST_CASE("dataset reader finds a source smaller than GDAL's probe spacing", "[dataset_reader]")
{
    auto reader = make_reader(make_raster({}, glm::uvec2(32), 1, GDT_Float32, { 100, 1, 0, 200, 0, -1 }), srs::Projection::WebMercator, { 1 }, 0.0F);
    const auto samples = read_samples(reader, { { 0, 0 }, { 4096, 4096 } }, glm::uvec2(4096));
    CHECK(samples.valid.pixel({ 116, 4096 - 184 }) != 0);
    CHECK(samples.data.pixel({ 116, 4096 - 184 }) == Catch::Approx(10));
}

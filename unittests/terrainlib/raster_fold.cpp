#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "raster/algorithm/fold.h"

TEST_CASE("fold visits raster and view pixels read-only in row order", "[raster-fold]")
{
    radix::Raster<int> values({ 3, 2 }, std::vector<int> { 1, 2, 3, 4, 5, 6 });
    const auto append = [](std::string state, const int& pixel) { return state + std::to_string(pixel); };
    CHECK(raster::algorithm::fold(values, std::string {}, append) == "123456");
    CHECK(raster::algorithm::fold(std::as_const(values), std::string {}, append) == "123456");
    auto view = raster::make_view(values, { 1, 0 }, { 2, 2 });
    REQUIRE(view);
    CHECK(raster::algorithm::fold(*view, std::string {}, append) == "2356");
    auto clamped = raster::make_clamped_view(values, { -1, 0 }, { 3, 2 });
    REQUIRE(clamped);
    CHECK(raster::algorithm::fold(*clamped, std::string {}, append) == "112445");
    CHECK(std::ranges::equal(values.buffer(), std::vector<int> { 1, 2, 3, 4, 5, 6 }));
}

TEST_CASE("fold supports empty rasters and move-only accumulators", "[raster-fold]")
{
    const auto sum = [](std::unique_ptr<int> state, const int& value) {
        *state += value;
        return state;
    };
    radix::Raster<int> empty;
    CHECK(*raster::algorithm::fold(empty, std::make_unique<int>(42), sum) == 42);
    radix::Raster<int> values({ 2, 2 }, std::vector<int> { 1, 2, 3, 4 });
    CHECK(*raster::algorithm::fold(values, std::make_unique<int>(5), sum) == 15);
}

TEST_CASE("fold accumulates finite min and max together", "[raster-fold]")
{
    const radix::Raster<double> values(
        { 3, 2 }, std::vector<double> { std::numeric_limits<double>::quiet_NaN(), -7, 12, 3, std::numeric_limits<double>::infinity(), -2 });
    const auto extrema = raster::algorithm::fold(values,
        std::pair { std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() },
        [](std::pair<double, double> state, const double& value) {
            if (std::isfinite(value)) {
                state.first = (std::min)(state.first, value);
                state.second = (std::max)(state.second, value);
            }
            return state;
        });
    CHECK(extrema.first == -7);
    CHECK(extrema.second == 12);
}

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <numbers>
#include <span>
#include <type_traits>
#include <vector>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <glm/gtc/type_precision.hpp>

#include "raster/algorithm.h"

namespace {

namespace algorithm = raster::algorithm;
using algorithm::Filter;
using algorithm::Interpolation;
using algorithm::ValueMapping;
using Catch::Matchers::WithinAbs;

template <typename T>
radix::Raster<T> samples(glm::uvec2 size, std::initializer_list<T> values)
{
    return radix::Raster<T>(size, std::vector<T>(values));
}

template <typename T>
auto scale(const radix::Raster<T>& data,
    const radix::Raster<std::uint16_t>& attribution,
    int levels,
    unsigned halo = 0,
    Filter filter = Filter::Box,
    Interpolation interpolation = Interpolation::NearestNeighbour,
    ValueMapping mapping = ValueMapping::Linear)
{
    return algorithm::scale(data, attribution, halo, levels, interpolation, filter, mapping);
}

template <typename T>
void same_raster(const radix::Raster<T>& left, const radix::Raster<T>& right)
{
    REQUIRE(left.size() == right.size());
    CHECK(std::ranges::equal(left.bytes(), right.bytes()));
}

auto field(glm::uvec2 interior, unsigned halo, int offset_x = 0, int offset_y = 0)
{
    const auto size = interior + glm::uvec2(2 * halo);
    std::pair result { radix::Raster<float>(size), radix::Raster<std::uint16_t>(size) };
    for (unsigned y = 0; y < size.y; ++y) {
        for (unsigned x = 0; x < size.x; ++x) {
            const int gx = static_cast<int>(x) - static_cast<int>(halo) + offset_x;
            const int gy = static_cast<int>(y) - static_cast<int>(halo) + offset_y;
            result.first.pixel({ x, y }) = float(std::sin(gx * 0.7) + std::cos(gy * 0.3) + gx * 0.01);
            result.second.pixel({ x, y }) = (3 * gx + gy) % 11 == 0 ? 0 : (gx < 8 ? 1 : 2);
        }
    }
    return result;
}

double reference_lanczos(const radix::Raster<float>& data, const radix::Raster<std::uint16_t>& attribution, unsigned halo, unsigned x, unsigned y, int radius)
{
    const auto weight = [radius](double distance) {
        const auto sinc = [](double value) { return value == 0 ? 1. : std::sin(std::numbers::pi * value) / (std::numbers::pi * value); };
        return std::abs(distance) >= 2 * radius ? 0. : sinc(distance / 2) * sinc(distance / (2 * radius));
    };
    const double centre_x = halo + 2 * x + 0.5;
    const double centre_y = halo + 2 * y + 0.5;
    double sum = 0;
    double normalization = 0;
    for (unsigned row = 0; row < data.height(); ++row) {
        for (unsigned column = 0; column < data.width(); ++column) {
            if (attribution.pixel({ column, row }) == 0) {
                continue;
            }
            const double contribution = weight(column - centre_x) * weight(row - centre_y);
            sum += contribution * data.pixel({ column, row });
            normalization += contribution;
        }
    }
    return sum / normalization;
}

static_assert(std::same_as<algorithm::WorkingPixel<float>, float>);
static_assert(std::same_as<algorithm::WorkingPixel<std::uint8_t>, float>);
static_assert(std::same_as<algorithm::WorkingPixel<std::int16_t>, float>);
static_assert(std::same_as<algorithm::WorkingPixel<double>, double>);
static_assert(std::same_as<algorithm::WorkingPixel<std::int32_t>, double>);
static_assert(std::same_as<algorithm::WorkingPixel<std::uint64_t>, long double>);
static_assert(std::same_as<algorithm::WorkingPixel<glm::vec<4, std::uint16_t>>, glm::vec4>);
static_assert(std::same_as<algorithm::WorkingPixel<glm::vec<3, std::int64_t>>, glm::vec<3, long double>>);

} // namespace

TEST_CASE("raster halo queries use signed zoom levels and complete filter chains", "[raster-algorithm]")
{
    for (const auto interpolation : { Interpolation::NearestNeighbour, Interpolation::Bilinear }) {
        for (const auto filter : { Filter::Box, Filter::Lanczos2, Filter::Lanczos3, Filter::Lanczos4 }) {
            CHECK(algorithm::required_halo(0, interpolation, filter).value() == 0);
            CHECK(algorithm::required_halo(4, interpolation, filter).value() == (interpolation == Interpolation::Bilinear ? 1 : 0));
        }
    }
    for (int levels = 1; levels <= 4; ++levels) {
        CHECK(algorithm::required_halo(-levels, Interpolation::Bilinear, Filter::Box).value() == 0);
        CHECK(algorithm::required_halo(-levels, Interpolation::Bilinear, Filter::Lanczos2).value() == 3u * ((1u << levels) - 1));
        CHECK(algorithm::required_halo(-levels, Interpolation::Bilinear, Filter::Lanczos3).value() == 5u * ((1u << levels) - 1));
        CHECK(algorithm::required_halo(-levels, Interpolation::Bilinear, Filter::Lanczos4).value() == 7u * ((1u << levels) - 1));
    }
    CHECK_FALSE(algorithm::required_halo((std::numeric_limits<int>::min)(), Interpolation::Bilinear, Filter::Box));
    CHECK_FALSE(algorithm::required_halo((std::numeric_limits<int>::max)(), Interpolation::Bilinear, Filter::Box));
    CHECK_FALSE(algorithm::required_halo(0, static_cast<Interpolation>(99), Filter::Box));
    CHECK_FALSE(algorithm::required_halo(0, Interpolation::Bilinear, static_cast<Filter>(99)));
    CHECK(algorithm::required_halo(-31, Interpolation::Bilinear, Filter::Lanczos4).error().code() == Error::Code::ResourceExhausted);
}

TEST_CASE("raster scaling rejects invalid geometry and dimensions before allocation", "[raster-algorithm]")
{
    radix::Raster<float> data(glm::uvec2(2), 1.f);
    radix::Raster<std::uint16_t> attribution(glm::uvec2(2), 1);
    CHECK_FALSE(scale(data, radix::Raster<std::uint16_t>(3), 0));
    CHECK_FALSE(scale(radix::Raster<float>(), radix::Raster<std::uint16_t>(), 0));
    CHECK_FALSE(scale(data, attribution, 0, 1));
    CHECK_FALSE(scale(data, attribution, 0, (std::numeric_limits<unsigned>::max)()));
    CHECK_FALSE(scale(data, attribution, -2));
    CHECK_FALSE(scale(data, attribution, 1, 0, Filter::Box, Interpolation::Bilinear));
    CHECK_FALSE(scale(data, attribution, -1, 0, Filter::Lanczos2));
    CHECK_FALSE(scale(data, attribution, 0, 0, Filter::Box, Interpolation::Bilinear, static_cast<ValueMapping>(99)));
    CHECK(scale(data, attribution, 0, 0, Filter::Box, Interpolation::Bilinear, ValueMapping::SRGBA).error().code() == Error::Code::Unsupported);
    CHECK(scale(data, attribution, 31).error().code() == Error::Code::ResourceExhausted);
    CHECK_FALSE(algorithm::reduce(data, attribution, 0, (std::numeric_limits<unsigned>::max)(), ValueMapping::Linear, algorithm::Max {}));
}

TEST_CASE("zero zoom levels crop both rasters preserving payload bits", "[raster-algorithm]")
{
    auto [data, attribution] = field({ 3, 2 }, 2);
    data.pixel({ 2, 2 }) = std::bit_cast<float>(std::uint32_t(0x7fc01234));
    data.pixel({ 3, 2 }) = -0.f;
    attribution.pixel({ 2, 2 }) = 0;
    auto output = scale(data, attribution, 0, 2, Filter::Lanczos4, Interpolation::Bilinear);
    REQUIRE(output);
    CHECK(output->first.size() == glm::uvec2(3, 2));
    for (unsigned y = 0; y < 2; ++y) {
        for (unsigned x = 0; x < 3; ++x) {
            CHECK(std::bit_cast<std::uint32_t>(output->first.pixel({ x, y })) == std::bit_cast<std::uint32_t>(data.pixel({ x + 2, y + 2 })));
            CHECK(output->second.pixel({ x, y }) == attribution.pixel({ x + 2, y + 2 }));
        }
    }
    unsigned calls = 0;
    const auto reducer = [&calls](std::span<const float>) {
        ++calls;
        return 0.f;
    };
    auto reduced = algorithm::reduce(data, attribution, 2, 0, ValueMapping::Linear, reducer);
    REQUIRE(reduced);
    CHECK(calls == 0);
    same_raster(output->first, reduced->first);
    same_raster(output->second, reduced->second);
}

TEST_CASE("nearest raster upscaling copies payloads and attribution directly", "[raster-algorithm]")
{
    const auto nan = std::bit_cast<float>(std::uint32_t(0x7fc03210));
    const auto data = samples<float>({ 3, 2 }, { 1, nan, -0.f, 4, 5, 6 });
    const auto attribution = samples<std::uint16_t>({ 3, 2 }, { 7, 0, 9, 4, 5, 6 });
    auto result = scale(data, attribution, 2);
    REQUIRE(result);
    CHECK(result->first.size() == glm::uvec2(12, 8));
    for (unsigned y = 0; y < 8; ++y) {
        for (unsigned x = 0; x < 12; ++x) {
            CHECK(std::bit_cast<std::uint32_t>(result->first.pixel({ x, y })) == std::bit_cast<std::uint32_t>(data.pixel({ x / 4, y / 4 })));
            CHECK(result->second.pixel({ x, y }) == attribution.pixel({ x / 4, y / 4 }));
        }
    }
}

TEST_CASE("bilinear raster upscaling respects area phase and nearest attribution", "[raster-algorithm]")
{
    radix::Raster<double> data(glm::uvec2(5, 4));
    radix::Raster<std::uint16_t> attribution(glm::uvec2(5, 4));
    for (unsigned y = 0; y < 4; ++y) {
        for (unsigned x = 0; x < 5; ++x) {
            data.pixel({ x, y }) = 2. * x + 3. * y;
            attribution.pixel({ x, y }) = static_cast<std::uint16_t>(1 + x + 5 * y);
        }
    }
    const auto output = scale(data, attribution, 2, 1, Filter::Box, Interpolation::Bilinear);
    REQUIRE(output);
    CHECK(output->first.size() == glm::uvec2(12, 8));
    for (unsigned y = 0; y < 8; ++y) {
        for (unsigned x = 0; x < 12; ++x) {
            const double expected = 2 * (1 + (x + 0.5) / 4 - 0.5) + 3 * (1 + (y + 0.5) / 4 - 0.5);
            CHECK_THAT(output->first.pixel({ x, y }), WithinAbs(expected, 1e-12));
            CHECK(output->second.pixel({ x, y }) == attribution.pixel({ x / 4 + 1, y / 4 + 1 }));
        }
    }
}

TEST_CASE("bilinear interpolation excludes NoData and preserves nearest NoData", "[raster-algorithm]")
{
    radix::Raster<float> data(glm::uvec2(3), std::numeric_limits<float>::quiet_NaN());
    radix::Raster<std::uint16_t> attribution(glm::uvec2(3), 0);
    data.pixel({ 1, 1 }) = 17;
    attribution.pixel({ 1, 1 }) = 9;
    const auto valid = scale(data, attribution, 2, 1, Filter::Box, Interpolation::Bilinear);
    REQUIRE(valid);
    for (auto value : valid->first) {
        CHECK(value == 17);
    }
    for (auto id : valid->second) {
        CHECK(id == 9);
    }
    data.fill(100);
    attribution.fill(2);
    data.pixel({ 1, 1 }) = -999;
    attribution.pixel({ 1, 1 }) = 0;
    const auto invalid = scale(data, attribution, 1, 1, Filter::Box, Interpolation::Bilinear);
    REQUIRE(invalid);
    CHECK(std::ranges::all_of(invalid->first, [](float value) { return value == -999; }));
    CHECK(std::ranges::all_of(invalid->second, [](auto id) { return id == 0; }));
}

TEST_CASE("box reduction votes locally and ignores invalid payloads", "[raster-algorithm]")
{
    const auto data = samples<float>({ 2, 2 }, { 2, std::numeric_limits<float>::quiet_NaN(), 6, 10 });
    auto attribution = samples<std::uint16_t>({ 2, 2 }, { 1, 0, 2, 2 });
    const auto result = scale(data, attribution, -1);
    REQUIRE(result);
    CHECK(result->first.pixel({ 0, 0 }) == 6);
    CHECK(result->second.pixel({ 0, 0 }) == 2);
    attribution = samples<std::uint16_t>({ 2, 2 }, { 5, 0, 3, 9 });
    CHECK(scale(data, attribution, -1)->second.pixel({ 0, 0 }) == 5);
    attribution = samples<std::uint16_t>({ 2, 2 }, { 5, 3, 3, 5 });
    CHECK(scale(data, attribution, -1)->second.pixel({ 0, 0 }) == 5);
    attribution.fill(0);
    const auto empty = scale(data, attribution, -1);
    REQUIRE(empty);
    CHECK(empty->second.pixel({ 0, 0 }) == 0);
    CHECK(empty->first.pixel({ 0, 0 }) == 10);
}

TEST_CASE("repeated reduction uses stage validity rounding and attribution", "[raster-algorithm]")
{
    radix::Raster<std::uint16_t> data(glm::uvec2(4), 0);
    radix::Raster<std::uint16_t> attribution(glm::uvec2(4), 0);
    attribution.pixel({ 0, 0 }) = 5;
    for (unsigned y = 0; y < 2; ++y) {
        for (unsigned x = 2; x < 4; ++x) {
            data.pixel({ x, y }) = 100;
            attribution.pixel({ x, y }) = 8;
        }
    }
    const auto direct = scale(data, attribution, -2);
    REQUIRE(direct);
    CHECK(direct->first.pixel({ 0, 0 }) == 50);
    CHECK(direct->second.pixel({ 0, 0 }) == 5);
    const auto first = scale(data, attribution, -1);
    REQUIRE(first);
    const auto second = scale(first->first, first->second, -1);
    REQUIRE(second);
    same_raster(direct->first, second->first);
    same_raster(direct->second, second->second);
}

TEST_CASE("custom raster reducers receive valid decoded spans and preserve callable state", "[raster-algorithm]")
{
    const auto data = samples<std::uint16_t>({ 2, 2 }, { 1, 999, 3, 4 });
    auto attribution = samples<std::uint16_t>({ 2, 2 }, { 5, 0, 7, 7 });
    auto calls = std::make_unique<unsigned>(0);
    const auto reducer = [&calls](std::span<const float> values) {
        ++*calls;
        REQUIRE(values.size() == 3);
        CHECK(values[0] == 1);
        CHECK(values[1] == 3);
        CHECK(values[2] == 4);
        return 2.5f;
    };
    const auto result = algorithm::reduce(data, attribution, 0, 1, ValueMapping::Linear, reducer);
    REQUIRE(result);
    CHECK(result->first.pixel({ 0, 0 }) == 3);
    CHECK(result->second.pixel({ 0, 0 }) == 7);
    CHECK(*calls == 1);
    attribution.fill(0);
    REQUIRE(algorithm::reduce(data, attribution, 0, 1, ValueMapping::Linear, reducer));
    CHECK(*calls == 1);

    struct Counter {
        std::unique_ptr<unsigned> count = std::make_unique<unsigned>(0);
        float operator()(std::span<const float> values)
        {
            ++*count;
            return algorithm::Max {}(values);
        }
    } counter;
    radix::Raster<float> larger(glm::uvec2(4), 42);
    radix::Raster<std::uint16_t> ids(glm::uvec2(4), 1);
    const auto repeated = algorithm::reduce(larger, ids, 0, 2, ValueMapping::Linear, counter);
    REQUIRE(repeated);
    CHECK(*counter.count == 5);
    CHECK(repeated->first.pixel({ 0, 0 }) == 42);
    REQUIRE(algorithm::reduce(larger, ids, 0, 2, ValueMapping::Linear, Counter {}));
}

TEST_CASE("provided raster reducers are component-wise with even median and NaN propagation", "[raster-algorithm]")
{
    const auto data = samples<glm::vec3>({ 2, 2 }, { { 8, 1, 10 }, { 2, 7, 20 }, { 6, 3, 30 }, { 4, 5, 40 } });
    radix::Raster<std::uint16_t> ids(glm::uvec2(2), 1);
    CHECK(algorithm::reduce(data, ids, 0, 1, ValueMapping::Linear, algorithm::Min {})->first.pixel({ 0, 0 }) == glm::vec3(2, 1, 10));
    CHECK(algorithm::reduce(data, ids, 0, 1, ValueMapping::Linear, algorithm::Max {})->first.pixel({ 0, 0 }) == glm::vec3(8, 7, 40));
    CHECK(algorithm::reduce(data, ids, 0, 1, ValueMapping::Linear, algorithm::Median {})->first.pixel({ 0, 0 }) == glm::vec3(5, 4, 25));
    ids.pixel({ 1, 1 }) = 0;
    CHECK(algorithm::reduce(data, ids, 0, 1, ValueMapping::Linear, algorithm::Median {})->first.pixel({ 0, 0 }) == glm::vec3(6, 3, 20));
    auto nonfinite = data;
    nonfinite.pixel({ 0, 0 }).y = std::numeric_limits<float>::quiet_NaN();
    const auto check_nan = [&](auto reducer) {
        auto result = algorithm::reduce(nonfinite, ids, 0, 1, ValueMapping::Linear, reducer);
        REQUIRE(result);
        CHECK(std::isfinite(result->first.pixel({ 0, 0 }).x));
        CHECK(std::isnan(result->first.pixel({ 0, 0 }).y));
    };
    check_nan(algorithm::Min {});
    check_nan(algorithm::Max {});
    check_nan(algorithm::Median {});
    const auto replace_nan = [](std::span<const glm::vec3> values) {
        CHECK(std::isnan(values[0].y));
        return glm::vec3(42);
    };
    CHECK(algorithm::reduce(nonfinite, ids, 0, 1, ValueMapping::Linear, replace_nan)->first.pixel({ 0, 0 }) == glm::vec3(42));
}

TEMPLATE_TEST_CASE("raster scalar working types support integer endpoints",
    "[raster-algorithm]",
    std::int8_t,
    std::uint8_t,
    std::int16_t,
    std::uint16_t,
    std::int32_t,
    std::uint32_t,
    std::int64_t,
    std::uint64_t,
    float,
    double)
{
    radix::Raster<std::uint16_t> ids(glm::uvec2(2), 1);
    for (const auto value : { (std::numeric_limits<TestType>::lowest)(), (std::numeric_limits<TestType>::max)() }) {
        radix::Raster<TestType> data(glm::uvec2(2), value);
        const auto result = scale(data, ids, -1);
        REQUIRE(result);
        CHECK(result->first.pixel({ 0, 0 }) == value);
        const auto maximum = algorithm::reduce(data, ids, 0, 1, ValueMapping::Linear, algorithm::Max {});
        REQUIRE(maximum);
        CHECK(maximum->first.pixel({ 0, 0 }) == value);
    }
}

TEST_CASE("integer raster conversion rounds clamps and rejects nonfinite custom values", "[raster-algorithm]")
{
    radix::Raster<std::int16_t> data(glm::uvec2(2), 0);
    radix::Raster<std::uint16_t> ids(glm::uvec2(2), 1);
    const auto convert = [&](float value) {
        const auto reducer = [value](std::span<const float>) { return value; };
        return algorithm::reduce(data, ids, 0, 1, ValueMapping::Linear, reducer);
    };
    CHECK(convert(0.5f)->first.pixel({ 0, 0 }) == 1);
    CHECK(convert(-0.5f)->first.pixel({ 0, 0 }) == -1);
    CHECK(convert(1e20f)->first.pixel({ 0, 0 }) == 32767);
    CHECK(convert(-1e20f)->first.pixel({ 0, 0 }) == -32768);
    CHECK(convert(std::numeric_limits<float>::infinity()).error().code() == Error::Code::InvalidInput);
    CHECK(convert(std::numeric_limits<float>::quiet_NaN()).error().code() == Error::Code::InvalidInput);
    radix::Raster<std::uint64_t> large(glm::uvec2(2), 0);
    const auto upper = [](std::span<const long double>) { return std::ldexp(1.L, 64); };
    CHECK(algorithm::reduce(large, ids, 0, 1, ValueMapping::Linear, upper)->first.pixel({ 0, 0 }) == (std::numeric_limits<std::uint64_t>::max)());
}

TEST_CASE("sRGB raster scaling and custom reduction operate in linear light with independent alpha", "[raster-algorithm]")
{
    const auto data = samples<glm::u8vec4>({ 2, 2 }, { { 0, 0, 0, 255 }, { 255, 255, 255, 0 }, { 0, 0, 0, 128 }, { 255, 255, 255, 127 } });
    radix::Raster<std::uint16_t> ids(glm::uvec2(2), 1);
    const auto result = scale(data, ids, -1, 0, Filter::Box, Interpolation::NearestNeighbour, ValueMapping::SRGBA);
    REQUIRE(result);
    CHECK(result->first.pixel({ 0, 0 }) == glm::u8vec4(188, 188, 188, 128));
    const auto reducer = [](std::span<const glm::vec4> values) {
        CHECK(values[0].x == 0);
        CHECK(values[1].x == 1);
        CHECK(values[2].w == 128.f / 255.f);
        return algorithm::Median {}(values);
    };
    const auto custom = algorithm::reduce(data, ids, 0, 1, ValueMapping::SRGBA, reducer);
    REQUIRE(custom);
    same_raster(custom->first, result->first);
    const auto nearest = scale(data, ids, 1, 0, Filter::Box, Interpolation::NearestNeighbour, ValueMapping::SRGBA);
    REQUIRE(nearest);
    CHECK(nearest->first.pixel({ 2, 0 }) == data.pixel({ 1, 0 }));

    radix::Raster<glm::u8vec3> rgb(glm::uvec2(6), glm::u8vec3(0));
    radix::Raster<std::uint16_t> rgb_ids(glm::uvec2(6), 1);
    for (unsigned y = 0; y < 6; ++y) {
        for (unsigned x = 0; x < 6; ++x) {
            rgb.pixel({ x, y }) = glm::u8vec3((13 * x + 79 * y) % 256);
        }
    }
    const auto direct = scale(rgb, rgb_ids, -2, 1, Filter::Box, Interpolation::NearestNeighbour, ValueMapping::SRGBA);
    const auto first = scale(rgb, rgb_ids, -1, 1, Filter::Box, Interpolation::NearestNeighbour, ValueMapping::SRGBA);
    REQUIRE(direct);
    REQUIRE(first);
    const auto second = scale(first->first, first->second, -1, 0, Filter::Box, Interpolation::NearestNeighbour, ValueMapping::SRGBA);
    REQUIRE(second);
    same_raster(direct->first, second->first);
}

TEST_CASE("Lanczos reduction matches an independent two-dimensional validity-aware reference", "[raster-algorithm]")
{
    for (int radius = 2; radius <= 4; ++radius) {
        const auto filter = radius == 2 ? Filter::Lanczos2 : (radius == 3 ? Filter::Lanczos3 : Filter::Lanczos4);
        const unsigned halo = 2 * radius - 1;
        auto [data, ids] = field({ 6, 4 }, halo);
        const auto result = scale(data, ids, -1, halo, filter);
        REQUIRE(result);
        for (unsigned y = 0; y < 2; ++y) {
            for (unsigned x = 0; x < 3; ++x) {
                CHECK_THAT(result->first.pixel({ x, y }), WithinAbs(reference_lanczos(data, ids, halo, x, y, radius), 3e-6));
            }
        }
        CHECK_FALSE(scale(data, ids, -1, halo - 1, filter));
    }
}

TEST_CASE("Lanczos discards valid outer support when the local block is NoData", "[raster-algorithm]")
{
    radix::Raster<float> data(glm::uvec2(8), 100);
    radix::Raster<std::uint16_t> ids(glm::uvec2(8), 1);
    for (unsigned y = 3; y < 5; ++y) {
        for (unsigned x = 3; x < 5; ++x) {
            ids.pixel({ x, y }) = 0;
            data.pixel({ x, y }) = -17;
        }
    }
    const auto result = scale(data, ids, -1, 3, Filter::Lanczos2);
    REQUIRE(result);
    CHECK(result->second.pixel({ 0, 0 }) == 0);
    CHECK(result->first.pixel({ 0, 0 }) == -17);
}

TEST_CASE("repeated Lanczos retains intermediate halo and matches separately supplied stages", "[raster-algorithm]")
{
    auto [data, ids] = field({ 8, 4 }, 15);
    const auto direct = scale(data, ids, -2, 15, Filter::Lanczos3);
    REQUIRE(direct);
    // Treat the extra source region as interior in the first call, retaining
    // five output pixels around the original interior for the second call.
    const auto first = scale(data, ids, -1, 5, Filter::Lanczos3);
    REQUIRE(first);
    const auto second = scale(first->first, first->second, -1, 5, Filter::Lanczos3);
    REQUIRE(second);
    same_raster(direct->first, second->first);
    same_raster(direct->second, second->second);
    auto [too_small, small_ids] = field({ 8, 4 }, 14);
    CHECK_FALSE(scale(too_small, small_ids, -2, 14, Filter::Lanczos3));
}

TEST_CASE("independent area tiles match their metatile through repeated scaling", "[raster-algorithm]")
{
    for (const auto filter : { Filter::Box, Filter::Lanczos2, Filter::Lanczos3, Filter::Lanczos4 }) {
        const auto halo = algorithm::required_halo(-2, Interpolation::Bilinear, filter).value();
        auto [whole, whole_ids] = field({ 16, 8 }, halo);
        const auto reference = scale(whole, whole_ids, -2, halo, filter);
        REQUIRE(reference);
        for (unsigned tile = 0; tile < 2; ++tile) {
            auto [data, ids] = field({ 8, 8 }, halo, int(tile * 8));
            const auto result = scale(data, ids, -2, halo, filter);
            REQUIRE(result);
            for (unsigned y = 0; y < 2; ++y) {
                for (unsigned x = 0; x < 2; ++x) {
                    CHECK(result->first.pixel({ x, y }) == reference->first.pixel({ x + tile * 2, y }));
                    CHECK(result->second.pixel({ x, y }) == reference->second.pixel({ x + tile * 2, y }));
                }
            }
        }
    }
}

TEST_CASE("Lanczos reduction preserves constants and suppresses high frequencies", "[raster-algorithm]")
{
    for (const auto filter : { Filter::Lanczos2, Filter::Lanczos3, Filter::Lanczos4 }) {
        const auto halo = algorithm::required_halo(-1, Interpolation::Bilinear, filter).value();
        const glm::uvec2 size(32 + 2 * halo, 2 + 2 * halo);
        radix::Raster<float> data(size, 7.f);
        radix::Raster<std::uint16_t> ids(size, 1);
        const auto constant = scale(data, ids, -1, halo, filter);
        REQUIRE(constant);
        for (const auto value : constant->first) {
            CHECK_THAT(value, WithinAbs(7, 3e-5));
        }
        for (unsigned y = 0; y < size.y; ++y) {
            for (unsigned x = 0; x < size.x; ++x) {
                data.pixel({ x, y }) = float(std::cos(0.8 * std::numbers::pi * (int(x) - int(halo))));
            }
        }
        const auto filtered = scale(data, ids, -1, halo, filter);
        REQUIRE(filtered);
        for (const auto value : filtered->first) {
            CHECK(std::abs(value) < 0.03f);
        }
    }
}

TEST_CASE("weight normalization falls back on cancellation but allows negative sums", "[raster-algorithm]")
{
    const auto data = samples<float>({ 2, 2 }, { 2, 4, 0, 0 });
    const auto ids = samples<std::uint16_t>({ 2, 2 }, { 1, 1, 0, 0 });
    const auto local = algorithm::detail::local_block(ids, { 0, 0 });
    algorithm::detail::Kernel<float> kernel(0);
    // Synthetic weights isolate the numerical threshold independently of
    // the selected Lanczos radius and the platform's sine approximation.
    kernel.weights[1] = -1 + std::numeric_limits<float>::epsilon();
    CHECK(kernel(data, ids, { 0, 0 }, local, ValueMapping::Linear).value() == 2);
    kernel.weights[1] = -2;
    CHECK(kernel(data, ids, { 0, 0 }, local, ValueMapping::Linear).value() == 6);
}

TEMPLATE_TEST_CASE("raster vector arithmetic instantiates every working precision", "[raster-algorithm]", glm::u16vec4, glm::i32vec3, glm::u64vec2, glm::dvec4)
{
    radix::Raster<TestType> data(glm::uvec2(8), TestType(12));
    radix::Raster<std::uint16_t> ids(glm::uvec2(8), 1);
    const auto filtered = scale(data, ids, -1, 3, Filter::Lanczos2);
    const auto median = algorithm::reduce(data, ids, 3, 1, ValueMapping::Linear, algorithm::Median {});
    REQUIRE(filtered);
    REQUIRE(median);
    for (glm::length_t i = 0; i < TestType::length(); ++i) {
        CHECK_THAT(static_cast<double>(filtered->first.pixel({ 0, 0 })[i]), WithinAbs(12, 1e-12));
        CHECK(median->first.pixel({ 0, 0 })[i] == 12);
    }
}

TEST_CASE("weighted raster filters preserve attributed nonfinite values and floating overshoot", "[raster-algorithm]")
{
    radix::Raster<float> data(glm::uvec2(2), 1);
    radix::Raster<std::uint16_t> ids(glm::uvec2(2), 1);
    data.pixel({ 0, 0 }) = std::numeric_limits<float>::quiet_NaN();
    const auto nan = scale(data, ids, -1);
    REQUIRE(nan);
    CHECK(std::isnan(nan->first.pixel({ 0, 0 })));
    CHECK(nan->second.pixel({ 0, 0 }) == 1);
    data.pixel({ 0, 0 }) = std::numeric_limits<float>::infinity();
    const auto infinite = scale(data, ids, -1);
    REQUIRE(infinite);
    CHECK(std::isinf(infinite->first.pixel({ 0, 0 })));

    radix::Raster<float> impulse(glm::uvec2(8), 0);
    radix::Raster<std::uint16_t> impulse_ids(glm::uvec2(8), 1);
    impulse.pixel({ 1, 3 }) = 1;
    const auto ringing = scale(impulse, impulse_ids, -1, 3, Filter::Lanczos2);
    REQUIRE(ringing);
    CHECK(ringing->first.pixel({ 0, 0 }) < 0);
    CHECK_THAT(ringing->first.pixel({ 0, 0 }), WithinAbs(reference_lanczos(impulse, impulse_ids, 3, 0, 0, 2), 1e-7));
    radix::Raster<std::uint8_t> integer_impulse(glm::uvec2(8), 0);
    integer_impulse.pixel({ 1, 3 }) = 255;
    const auto clamped = scale(integer_impulse, impulse_ids, -1, 3, Filter::Lanczos2);
    REQUIRE(clamped);
    CHECK(clamped->first.pixel({ 0, 0 }) == 0);
}

TEST_CASE("bilinear colour interpolation decodes sRGB before combining samples", "[raster-algorithm]")
{
    radix::Raster<glm::u8vec3> data(glm::uvec2(3), glm::u8vec3(255));
    radix::Raster<std::uint16_t> ids(glm::uvec2(3), 1);
    data.pixel({ 1, 1 }) = glm::u8vec3(0);
    const auto output = scale(data, ids, 1, 1, Filter::Box, Interpolation::Bilinear, ValueMapping::SRGBA);
    REQUIRE(output);
    const auto expected = std::round(255 * (1.055 * std::pow(1. - 0.75 * 0.75, 1. / 2.4) - 0.055));
    for (const auto value : output->first) {
        CHECK(value == glm::u8vec3(static_cast<std::uint8_t>(expected)));
    }
}

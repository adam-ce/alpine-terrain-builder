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
#include "raster_store/scaler.h"

namespace {

namespace algorithm = raster::algorithm;
using algorithm::Filter;
using algorithm::Interpolation;
namespace scaler = raster_store::scaler;
using ValueMapping = raster_store::pixel::Mapping;
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
    return scaler::scale(data, attribution, halo, levels, interpolation, filter, mapping);
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

double reference_lanczos(const radix::Raster<float>& data, const radix::Raster<std::uint16_t>&, unsigned halo, unsigned x, unsigned y, int radius)
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
            const double contribution = weight(column - centre_x) * weight(row - centre_y);
            sum += contribution * data.pixel({ column, row });
            normalization += contribution;
        }
    }
    return sum / normalization;
}

static_assert(std::same_as<algorithm::detail::LinearPixel<float>, float>);
static_assert(std::same_as<algorithm::detail::LinearPixel<std::uint8_t>, float>);
static_assert(std::same_as<algorithm::detail::LinearPixel<std::int16_t>, float>);
static_assert(std::same_as<algorithm::detail::LinearPixel<double>, double>);
static_assert(std::same_as<algorithm::detail::LinearPixel<std::int32_t>, double>);
static_assert(std::same_as<algorithm::detail::LinearPixel<std::uint64_t>, long double>);
static_assert(std::same_as<algorithm::detail::LinearPixel<glm::vec<4, std::uint16_t>>, glm::vec4>);
static_assert(std::same_as<algorithm::detail::LinearPixel<glm::vec<3, std::int64_t>>, glm::vec<3, long double>>);

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
    CHECK_FALSE(scaler::reduce(data, attribution, 0, (std::numeric_limits<unsigned>::max)(), algorithm::Max {}));
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
    auto reduced = scaler::reduce(data, attribution, 2, 0, algorithm::linear_conversion<algorithm::detail::SourcePixel<decltype(data)>>(), reducer);
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

TEST_CASE("bilinear interpolation includes unattributed samples", "[raster-algorithm]")
{
    radix::Raster<float> data(glm::uvec2(3), 100);
    radix::Raster<std::uint16_t> ids(glm::uvec2(3), 0);
    data.pixel({ 1, 1 }) = 0;
    const auto result = scale(data, ids, 1, 1, Filter::Box, Interpolation::Bilinear);
    REQUIRE(result);
    for (const auto value : result->first) {
        CHECK(value == 43.75f);
    }
    for (const auto id : result->second) {
        CHECK(id == 0);
    }
    same_raster(result->first, algorithm::scale(data, 1, 1, Interpolation::Bilinear, Filter::Box).value());
}

TEST_CASE("box reduction votes locally and includes every payload", "[raster-algorithm]")
{
    const auto data = samples<float>({ 2, 2 }, { 2, 6, 6, 10 });
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
    CHECK(empty->first.pixel({ 0, 0 }) == 6);
}

TEST_CASE("repeated reduction uses stage rounding and attribution", "[raster-algorithm]")
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
    CHECK(direct->first.pixel({ 0, 0 }) == 25);
    CHECK(direct->second.pixel({ 0, 0 }) == 0);
    const auto first = scale(data, attribution, -1);
    REQUIRE(first);
    const auto second = scale(first->first, first->second, -1);
    REQUIRE(second);
    same_raster(direct->first, second->first);
    same_raster(direct->second, second->second);
}

TEST_CASE("custom reducers receive four decoded row-major samples and immutable configuration", "[raster-algorithm]")
{
    const auto data = samples<std::uint16_t>({ 2, 2 }, { 1, 999, 3, 4 });
    radix::Raster<std::uint16_t> ids(glm::uvec2(2), 0);
    const auto conversion = std::tuple { [](const std::uint16_t& value) { return double(value) * 2; },
        [](const double& value) { return static_cast<std::uint16_t>(value / 2); } };
    const auto reducer = [bias = 2.](std::span<const double> values) {
        REQUIRE(values.size() == 4);
        CHECK(values[0] == 2);
        CHECK(values[1] == 1998);
        CHECK(values[2] == 6);
        CHECK(values[3] == 8);
        return values[2] + bias;
    };
    const auto result = scaler::reduce(data, ids, 0, 1, conversion, reducer);
    REQUIRE(result);
    CHECK(result->first.pixel({ 0, 0 }) == 4);
    CHECK(result->second.pixel({ 0, 0 }) == 0);
    CHECK(algorithm::reduce(data, 0, 1, algorithm::Max {})->pixel({ 0, 0 }) == 999);
}

TEST_CASE("provided raster reducers are component-wise with even median", "[raster-algorithm]")
{
    const auto data = samples<glm::vec3>({ 2, 2 }, { { 8, 1, 10 }, { 2, 7, 20 }, { 6, 3, 30 }, { 4, 5, 40 } });
    CHECK(algorithm::reduce(data, 0, 1, algorithm::Min {})->pixel({ 0, 0 }) == glm::vec3(2, 1, 10));
    CHECK(algorithm::reduce(data, 0, 1, algorithm::Max {})->pixel({ 0, 0 }) == glm::vec3(8, 7, 40));
    CHECK(algorithm::reduce(data, 0, 1, algorithm::Median {})->pixel({ 0, 0 }) == glm::vec3(5, 4, 25));
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
        const auto maximum = scaler::reduce(data, ids, 0, 1, algorithm::Max {});
        REQUIRE(maximum);
        CHECK(maximum->first.pixel({ 0, 0 }) == value);
    }
}

TEST_CASE("integer raster conversion rounds and clamps finite custom values", "[raster-algorithm]")
{
    radix::Raster<std::int16_t> data(glm::uvec2(2), 0);
    radix::Raster<std::uint16_t> ids(glm::uvec2(2), 1);
    const auto convert = [&](float value) {
        const auto reducer = [value](std::span<const float>) { return value; };
        return scaler::reduce(data, ids, 0, 1, algorithm::linear_conversion<algorithm::detail::SourcePixel<decltype(data)>>(), reducer);
    };
    CHECK(convert(0.5f)->first.pixel({ 0, 0 }) == 1);
    CHECK(convert(-0.5f)->first.pixel({ 0, 0 }) == -1);
    CHECK(convert(1e20f)->first.pixel({ 0, 0 }) == 32767);
    CHECK(convert(-1e20f)->first.pixel({ 0, 0 }) == -32768);
    radix::Raster<std::uint64_t> large(glm::uvec2(2), 0);
    const auto upper = [](std::span<const long double>) { return std::ldexp(1.L, 64); };
    CHECK(scaler::reduce(large, ids, 0, 1, algorithm::linear_conversion<std::uint64_t>(), upper)->first.pixel({ 0, 0 })
        == (std::numeric_limits<std::uint64_t>::max)());
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
    const auto custom = scaler::reduce(data, ids, 0, 1, algorithm::srgb_conversion<glm::u8vec4>(), reducer);
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

TEST_CASE("Lanczos reduction matches an independent two-dimensional reference", "[raster-algorithm]")
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

TEST_CASE("Lanczos includes outer support when the local block is unattributed", "[raster-algorithm]")
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
    CHECK_THAT(result->first.pixel({ 0, 0 }), WithinAbs(reference_lanczos(data, ids, 3, 0, 0, 2), 3e-5));
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

TEMPLATE_TEST_CASE("raster vector arithmetic instantiates every working precision", "[raster-algorithm]", glm::u16vec4, glm::i32vec3, glm::u64vec2, glm::dvec4)
{
    radix::Raster<TestType> data(glm::uvec2(8), TestType(12));
    radix::Raster<std::uint16_t> ids(glm::uvec2(8), 1);
    const auto filtered = scale(data, ids, -1, 3, Filter::Lanczos2);
    const auto median = scaler::reduce(data, ids, 3, 1, algorithm::Median {});
    REQUIRE(filtered);
    REQUIRE(median);
    for (glm::length_t i = 0; i < TestType::length(); ++i) {
        CHECK_THAT(static_cast<double>(filtered->first.pixel({ 0, 0 })[i]), WithinAbs(12, 1e-12));
        CHECK(median->first.pixel({ 0, 0 })[i] == 12);
    }
}

TEST_CASE("weighted filters retain floating overshoot and clamp integers", "[raster-algorithm]")
{
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

TEMPLATE_TEST_CASE("identity integer median avoids overflow and rounds halves away from zero",
    "[raster-algorithm]",
    std::int16_t,
    std::uint16_t,
    std::int32_t,
    std::uint32_t,
    std::int64_t,
    std::uint64_t)
{
    using T = TestType;
    const auto check = [](T lower, T upper, T expected) {
        const auto data = samples<T>({ 2, 2 }, { lower, upper, lower, upper });
        const auto result = algorithm::reduce(data, 0, 1, algorithm::Median {});
        REQUIRE(result);
        CHECK(result->pixel({ 0, 0 }) == expected);
    };
    const T highest = (std::numeric_limits<T>::max)();
    const T lowest = (std::numeric_limits<T>::lowest)();
    check(highest, highest, highest);
    check(lowest, lowest, lowest);
    check(highest - 1, highest, highest);
    check(0, 1, 1);
    if constexpr (std::is_signed_v<T>) {
        check(-1, 0, -1);
        check(-2, 1, -1);
        check(-1, 2, 1);
        check(lowest, highest, -1);
        check(lowest, lowest + 1, lowest);
    }
}

TEST_CASE("mode treats zero as ordinary and equality ties select first occurrence", "[raster-algorithm]")
{
    const auto check = [](std::array<float, 4> values, float expected) {
        const float result = algorithm::Mode {}(std::span<const float>(values));
        if (std::isnan(expected)) {
            CHECK(std::isnan(result));
        } else {
            CHECK(result == expected);
        }
    };
    check({ 0, 0, 0, 7 }, 0);
    check({ 4, 2, 2, 4 }, 4);
    check({ 3, 0, 2, 4 }, 3);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    check({ nan, nan, nan, nan }, nan);
    check({ nan, 2, nan, 2 }, 2);
    check({ nan, 2, 3, 4 }, nan);
}

TEST_CASE("scalers accept ordinary and clamped views with matching destination results", "[raster-algorithm]")
{
    auto [backing, ids] = field({ 12, 8 }, 0);
    const auto source = raster::make_view(backing, { 2, 2 }, { 8, 4 }).value();
    const auto padded = raster::make_clamped_view(source, { -9, -9 }, { 26, 22 }).value();
    const auto materialized = algorithm::copy(padded).value();
    radix::Raster<float> output_backing(glm::uvec2(6, 5), -100);
    const auto output = raster::make_view(output_backing, { 2, 2 }, { 2, 1 }).value();
    for (const auto filter : { Filter::Box, Filter::Lanczos2 }) {
        const auto expected = algorithm::scale(materialized, 9, -2, Interpolation::Bilinear, filter);
        const auto actual = algorithm::scale(padded, 9, -2, Interpolation::Bilinear, filter);
        REQUIRE(expected);
        REQUIRE(actual);
        same_raster(*actual, *expected);
        REQUIRE(algorithm::scale(padded, 9, -2, Interpolation::Bilinear, filter, output));
        same_raster(algorithm::copy(output).value(), *expected);
    }
    CHECK(output_backing.pixel({ 0, 0 }) == -100);
    const auto reduced = algorithm::reduce(source, 0, 2, algorithm::Max {});
    REQUIRE(reduced);
    REQUIRE(algorithm::reduce(source, 0, 2, algorithm::Max {}, output));
    same_raster(algorithm::copy(output).value(), *reduced);
    const auto padded_reduction = algorithm::reduce(padded, 9, 2, algorithm::Max {});
    REQUIRE(padded_reduction);
    same_raster(*padded_reduction, *reduced);
    const auto up_padded = raster::make_clamped_view(source, { -1, -1 }, { 10, 6 }).value();
    const auto up = algorithm::scale(up_padded, 1, 1, Interpolation::Bilinear, Filter::Box);
    REQUIRE(up);
    radix::Raster<float> up_output(up->size());
    REQUIRE(algorithm::scale(up_padded, 1, 1, Interpolation::Bilinear, Filter::Box, up_output));
    same_raster(*up, up_output);
}

TEST_CASE("scaling validates geometry and overlap before invoking callables or writing", "[raster-algorithm]")
{
    radix::Raster<float> source(glm::uvec2(6), 1);
    radix::Raster<float> output(glm::uvec2(2), -10);
    const auto never = [](std::span<const float>) -> float {
        FAIL("invalid input must not invoke reducer");
        return 0;
    };
    CHECK_FALSE(algorithm::reduce(source, 0, 1, never, output));
    CHECK_FALSE(algorithm::reduce(source, 3, 1, never));
    CHECK_FALSE(algorithm::reduce(radix::Raster<float>(), 0, 0, never));
    const auto overlapping = raster::make_view(source, { 0, 0 }, { 2, 2 }).value();
    CHECK_FALSE(algorithm::reduce(source, 1, 1, never, overlapping));
    CHECK_FALSE(algorithm::scale(source, 1, -1, Interpolation::Bilinear, Filter::Box, overlapping));
    const auto identical_crop = raster::make_view(source, { 1, 1 }, { 4, 4 }).value();
    REQUIRE(algorithm::reduce(source, 1, 0, never, identical_crop));
    REQUIRE(algorithm::scale(source, 1, 0, Interpolation::Bilinear, Filter::Box, identical_crop));
    const auto partial_crop = raster::make_view(source, { 0, 0 }, { 4, 4 }).value();
    CHECK_FALSE(algorithm::reduce(source, 1, 0, never, partial_crop));
    const auto clamped = raster::make_clamped_view(source, { -1, -1 }, { 8, 8 }).value();
    CHECK_FALSE(algorithm::reduce(clamped, 2, 1, never, overlapping));
    CHECK(std::ranges::all_of(output, [](float value) { return value == -10; }));
}

TEST_CASE("paired destination validation prevents partial writes on invalid attribution output", "[raster-algorithm]")
{
    radix::Raster<std::uint16_t> data(glm::uvec2(4), 12);
    radix::Raster<std::uint16_t> ids(glm::uvec2(4), 0);
    radix::Raster<std::uint16_t> output(glm::uvec2(2), 99);
    radix::Raster<std::uint16_t> wrong(glm::uvec2(3), 77);
    CHECK_FALSE(scaler::scale(data, ids, 0, -1, Interpolation::Bilinear, Filter::Box, ValueMapping::Linear, output, wrong));
    CHECK_FALSE(scaler::reduce(data, ids, 0, 1, algorithm::Max {}, output, wrong));
    const auto cross = raster::make_view(data, { 0, 0 }, { 2, 2 }).value();
    CHECK_FALSE(scaler::reduce(data, ids, 0, 1, algorithm::Max {}, output, cross));
    CHECK_FALSE(scaler::scale(data, ids, 0, -1, Interpolation::Bilinear, Filter::Box, ValueMapping::Linear, output, cross));
    CHECK_FALSE(scaler::reduce(data, ids, 0, 1, algorithm::Max {}, output, output));
    CHECK(std::ranges::all_of(output, [](auto value) { return value == 99; }));
    radix::Raster<std::uint16_t> out_ids(glm::uvec2(2));
    REQUIRE(scaler::reduce(data, ids, 0, 1, algorithm::Max {}, output, out_ids));
    CHECK(std::ranges::all_of(output, [](auto value) { return value == 12; }));
    CHECK(std::ranges::all_of(out_ids, [](auto value) { return value == 0; }));
}

TEST_CASE("nearest and crop bypass custom conversion and reducers", "[raster-algorithm]")
{
    const auto bits = std::uint32_t(0x7fc01234);
    radix::Raster<float> data(glm::uvec2(2), std::bit_cast<float>(bits));
    const auto conversion = std::tuple { [](const float&) -> double {
                                            FAIL("copy must not decode");
                                            return 0;
                                        },
        [](const double&) -> float {
            FAIL("copy must not encode");
            return 0;
        } };
    for (const int level : { 0, 2 }) {
        const auto result = algorithm::scale(data, 0, level, Interpolation::NearestNeighbour, Filter::Box, conversion);
        REQUIRE(result);
        for (const auto value : *result) {
            CHECK(std::bit_cast<std::uint32_t>(value) == bits);
        }
    }
    const auto reduced = algorithm::reduce(data, 0, 0, conversion, [](std::span<const double>) -> double {
        FAIL("crop must not reduce");
        return 0;
    });
    REQUIRE(reduced);
    same_raster(data, *reduced);
}

TEST_CASE("separable filtering encodes once per two-dimensional reduction step", "[raster-algorithm]")
{
    const auto data = samples<std::int16_t>({ 2, 2 }, { 0, 1, 0, 0 });
    const auto rounded = algorithm::scale(data, 0, -1, Interpolation::NearestNeighbour, Filter::Box);
    REQUIRE(rounded);
    CHECK(rounded->pixel({ 0, 0 }) == 0); // Rounding horizontal rows first would produce 1.
    const auto conversion = std::tuple { [](const std::int16_t& value) { return double(value) + 10; },
        [](const double& value) { return static_cast<std::int16_t>(std::round(value - 10)); } };
    const auto custom = algorithm::scale(data, 0, -1, Interpolation::NearestNeighbour, Filter::Box, conversion);
    REQUIRE(custom);
    same_raster(*rounded, *custom);
}

namespace {
template <typename Conversion>
concept CanScaleFloat = requires(
    radix::Raster<float>& source, const Conversion& conversion) { algorithm::scale(source, 0, -1, Interpolation::NearestNeighbour, Filter::Box, conversion); };
template <typename Conversion>
concept CanReduceFloat
    = requires(radix::Raster<float>& source, const Conversion& conversion) { algorithm::reduce(source, 0, 1, conversion, algorithm::Max {}); };
using ExpectedDecoder = decltype(std::tuple { [](const float& v) -> Expected<float> { return v; }, [](const float& v) { return v; } });
using ExpectedEncoder = decltype(std::tuple { [](const float& v) { return v; }, [](const float& v) -> Expected<float> { return v; } });
using WrongEncoder = decltype(std::tuple { [](const float& v) { return double(v); }, [](const double& v) { return v; } });
using IntegerWorking = decltype(std::tuple { [](const float& v) { return int(v); }, [](const int& v) { return float(v); } });
static_assert(!CanScaleFloat<ExpectedDecoder> && !CanReduceFloat<ExpectedDecoder>);
static_assert(!CanScaleFloat<ExpectedEncoder> && !CanReduceFloat<ExpectedEncoder>);
static_assert(!CanScaleFloat<WrongEncoder> && !CanReduceFloat<WrongEncoder>);
static_assert(!CanScaleFloat<IntegerWorking> && CanReduceFloat<IntegerWorking>);
} // namespace

TEST_CASE("scaling invokes encoders with const working values", "[raster-algorithm]")
{
    struct Encoder {
        float operator()(const double& value) const { return static_cast<float>(value); }
        float operator()(double&) const
        {
            FAIL("encoder must receive a const working value");
            return 0;
        }
    };
    const auto conversion = std::tuple { [](const float& value) { return double(value); }, Encoder {} };
    radix::Raster<float> source(glm::uvec2(4), 7);
    const auto down = algorithm::scale(source, 0, -1, Interpolation::NearestNeighbour, Filter::Box, conversion);
    const auto up = algorithm::scale(source, 1, 1, Interpolation::Bilinear, Filter::Box, conversion);
    REQUIRE(down);
    REQUIRE(up);
    for (const auto value : *down) {
        CHECK(value == 7);
    }
    for (const auto value : *up) {
        CHECK(value == 7);
    }
}

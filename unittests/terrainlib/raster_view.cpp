#include <algorithm>
#include <bit>
#include <concepts>
#include <cstdint>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "raster/algorithm/copy.h"
#include "raster/algorithm/transform.h"
#include "raster/algorithm/window_transform.h"
#include "raster/algorithm/zip_transform.h"

namespace {

namespace algorithm = raster::algorithm;
using raster::make_clamped_view;
using raster::make_view;
using Raster = radix::Raster<int>;
using View = raster::View<int>;
using ConstView = raster::View<const int>;
using ClampedView = raster::ClampedView<int>;

template <typename Source>
concept CanMakeView = requires(Source&& source) { make_view(std::forward<Source>(source)); };

template <typename Source>
concept CanMakeRegion = requires(Source&& source) { make_view(std::forward<Source>(source), glm::i64vec2(0), glm::uvec2(1)); };

template <typename Source>
concept CanMakeClampedView = requires(Source&& source) { make_clamped_view(std::forward<Source>(source), glm::i64vec2(-1), glm::uvec2(3)); };

template <typename Source, typename Destination>
concept CanCopy
    = requires(Source&& source, Destination&& destination) { algorithm::copy(std::forward<Source>(source), std::forward<Destination>(destination)); };

template <typename Source, typename Function, typename Destination>
concept CanTransform = requires(Source&& source, const Function& function, Destination&& destination) {
    algorithm::transform(std::forward<Source>(source), function, std::forward<Destination>(destination));
};

template <typename Source, typename Function, typename Destination>
concept CanWindowTransform = requires(Source&& source, const Function& function, Destination&& destination) {
    algorithm::window_transform(std::forward<Source>(source), 3, function, std::forward<Destination>(destination));
};

template <typename Sources, typename Function, typename Destination>
concept CanZipTuple = requires(const Sources& sources, const Function& function, Destination&& destination) {
    algorithm::zip_transform(sources, function, std::forward<Destination>(destination));
};

template <typename Function>
concept CanAllocateTransform = requires(Raster& source, const Function& function) { algorithm::transform(source, function); };

struct Identity {
    int operator()(const int& value) const { return value; }
};

struct ReferenceResult {
    const int& operator()(const int& value) const { return value; }
};

struct MutableInput {
    int operator()(int& value) const { return ++value; }
};

struct Sum {
    template <typename... T>
    int operator()(const T&... values) const
    {
        return (values + ...);
    }
};

struct Centre {
    template <typename Window>
    int operator()(const Window& window) const
    {
        return window.pixel(window.size() / 2u);
    }
};

struct MutableWindow {
    int operator()(const View& window) const { return ++window.pixel({ 0, 0 }); }
};

static_assert(CanMakeView<Raster&> && CanMakeView<const Raster&>);
static_assert(!CanMakeView<Raster> && !CanMakeView<const Raster>);
static_assert(CanMakeView<View> && CanMakeView<const View&> && CanMakeView<ClampedView>);
static_assert(CanMakeRegion<Raster&> && CanMakeRegion<View> && CanMakeRegion<ClampedView>);
static_assert(!CanMakeRegion<Raster> && !CanMakeRegion<const Raster>);
static_assert(CanMakeClampedView<Raster&> && CanMakeClampedView<const Raster&> && CanMakeClampedView<View>);
static_assert(!CanMakeClampedView<Raster> && !CanMakeClampedView<const Raster>);
static_assert(std::is_convertible_v<View, ConstView> && !std::is_convertible_v<ConstView, View>);
static_assert(std::same_as<decltype(std::declval<const View&>().pixel({ 0, 0 })), int&>);
static_assert(std::same_as<decltype(std::declval<const ClampedView&>().pixel({ 0, 0 })), const int&>);
static_assert(CanCopy<Raster, Raster> && CanCopy<const Raster&, const View&> && CanCopy<ClampedView, Raster>);
static_assert(!CanCopy<Raster&, const Raster&> && !CanCopy<Raster&, ConstView> && !CanCopy<Raster&, ClampedView>);
static_assert(!CanCopy<Raster&, radix::Raster<float>&> && !CanCopy<int, Raster&>);
static_assert(CanTransform<Raster, Identity, Raster> && CanTransform<ClampedView, Identity, View>);
static_assert(!CanTransform<Raster&, Identity, ClampedView> && !CanTransform<Raster&, Identity, ConstView>);
static_assert(!CanTransform<Raster&, ReferenceResult, Raster&> && !CanTransform<Raster&, MutableInput, Raster&>);
static_assert(!CanTransform<Raster&, Identity, radix::Raster<float>&>);
static_assert(CanWindowTransform<Raster, Centre, View> && CanWindowTransform<ClampedView, Centre, Raster>);
static_assert(!CanWindowTransform<Raster&, MutableWindow, Raster&> && !CanWindowTransform<Raster&, Centre, ClampedView>);
static_assert(CanZipTuple<std::tuple<Raster&, View, ConstView, ClampedView>, Sum, Raster>);
static_assert(!CanZipTuple<std::tuple<View, View>, Sum, Raster> && !CanZipTuple<std::tuple<View, View, View>, Sum, Raster>);
static_assert(!CanZipTuple<std::tuple<View, View, View, View>, Sum, ClampedView>);
static_assert(CanAllocateTransform<Identity> && !CanAllocateTransform<ReferenceResult> && !CanAllocateTransform<MutableInput>);
static_assert(!CanAllocateTransform<decltype([](int) { return true; })>);
static_assert(!CanAllocateTransform<decltype([](int) { })>);

Raster numbered(glm::uvec2 size)
{
    Raster result(size);
    for (unsigned y = 0; y < size.y; ++y) {
        for (unsigned x = 0; x < size.x; ++x) {
            result.pixel({ x, y }) = int(10 * y + x);
        }
    }
    return result;
}

} // namespace

TEST_CASE("raster views preserve stride and compose relative regions", "[raster-view]")
{
    auto raster = numbered({ 7, 5 });
    auto region = make_view(raster, { 2, 1 }, { 4, 3 });
    REQUIRE(region);
    CHECK(region->size() == glm::uvec2(4, 3));
    CHECK(region->stride() == 7);
    CHECK(region->offset() == 9);
    auto nested = make_view(*region, { 1, 1 }, { 2, 2 });
    REQUIRE(nested);
    CHECK(nested->stride() == 7);
    CHECK(nested->offset() == 17);
    CHECK(nested->pixel({ 1, 1 }) == 34);
    const auto writable = *nested;
    writable.pixel({ 0, 0 }) = 99;
    CHECK(raster.pixel({ 3, 2 }) == 99);
    const ConstView read_only = writable;
    CHECK(&read_only.pixel({ 1, 1 }) == &raster.pixel({ 4, 3 }));
    CHECK_FALSE(make_view(*region, { 3, 0 }, { 2, 1 }));
    CHECK_FALSE(make_view(raster, { -1, 0 }, { 1, 1 }));
    CHECK_FALSE(make_view(raster, { (std::numeric_limits<std::int64_t>::max)(), 0 }, { 0, 0 }));
    CHECK_FALSE(make_view(raster, { 1, 0 }, { (std::numeric_limits<unsigned>::max)(), 1 }));
    auto empty = make_view(raster, { 7, 5 }, { 0, 0 });
    REQUIRE(empty);
    CHECK(empty->size() == glm::uvec2(0));
    CHECK(make_view(*empty, { 0, 0 }, { 0, 0 }));
}

TEST_CASE("clamped views repeat the supplied source region and preserve nested mapping", "[raster-view]")
{
    auto raster = numbered({ 6, 5 });
    auto source = make_view(raster, { 2, 1 }, { 2, 3 });
    REQUIRE(source);
    auto padded = make_clamped_view(*source, { -2, -1 }, { 6, 5 });
    REQUIRE(padded);
    for (unsigned y = 0; y < padded->height(); ++y) {
        for (unsigned x = 0; x < padded->width(); ++x) {
            const auto sx = unsigned(std::clamp(int(x) - 2, 0, 1)) + 2;
            const auto sy = unsigned(std::clamp(int(y) - 1, 0, 2)) + 1;
            CHECK(padded->pixel({ x, y }) == raster.pixel({ sx, sy }));
        }
    }
    auto nested = make_view(*padded, { 1, 1 }, { 4, 3 });
    REQUIRE(nested);
    auto twice = make_view(*nested, { 1, 0 }, { 3, 3 });
    REQUIRE(twice);
    for (unsigned y = 0; y < 3; ++y) {
        for (unsigned x = 0; x < 3; ++x) {
            CHECK(twice->pixel({ x, y }) == padded->pixel({ x + 2, y + 1 }));
        }
    }
    CHECK_FALSE(make_view(*padded, { -1, 0 }, { 1, 1 }));
    CHECK_FALSE(make_view(*padded, { 5, 0 }, { 2, 1 }));
    auto far = make_clamped_view(raster, { (std::numeric_limits<std::int64_t>::min)(), (std::numeric_limits<std::int64_t>::max)() }, { 4, 4 });
    REQUIRE(far);
    CHECK(far->pixel({ 0, 0 }) == 40);
    CHECK(far->pixel({ 3, 3 }) == 40);
    Raster empty;
    CHECK_FALSE(make_clamped_view(empty, { -1, -1 }, { 3, 3 }));
    CHECK(make_clamped_view(empty, { -1, -1 }, { 0, 3 }));
}

TEST_CASE("view copy handles raster wrappers strided regions and raw payloads", "[raster-view]")
{
    auto source = numbered({ 5, 4 });
    Raster destination({ 6, 5 }, -1);
    auto input = make_view(source, { 1, 1 }, { 3, 2 });
    auto output = make_view(destination, { 2, 2 }, { 3, 2 });
    REQUIRE(input);
    REQUIRE(output);
    REQUIRE(algorithm::copy(*input, *output));
    for (unsigned y = 0; y < destination.height(); ++y) {
        for (unsigned x = 0; x < destination.width(); ++x) {
            CHECK(destination.pixel({ x, y }) == (x >= 2 && x < 5 && y >= 2 && y < 4 ? int(10 * (y - 1) + x - 1) : -1));
        }
    }
    Raster whole(source.size());
    REQUIRE(algorithm::copy(source, whole));
    REQUIRE(algorithm::copy(source, make_view(whole)));
    REQUIRE(algorithm::copy(make_view(source), whole));
    CHECK(std::ranges::equal(source, whole));
    REQUIRE(algorithm::copy(numbered({ 5, 4 }), whole));
    REQUIRE(algorithm::copy(source, Raster(source.size())));

    radix::Raster<float> bits({ 2, 1 });
    bits.pixel({ 0, 0 }) = std::bit_cast<float>(std::uint32_t(0x7fc01234));
    bits.pixel({ 1, 0 }) = -0.f;
    radix::Raster<float> copied(bits.size());
    REQUIRE(algorithm::copy(bits, copied));
    CHECK(std::ranges::equal(bits.bytes(), copied.bytes()));
}

TEST_CASE("copy checks actual rectangular overlap before writing", "[raster-view]")
{
    auto raster = numbered({ 6, 4 });
    const auto original = raster;
    auto left = make_view(raster, { 0, 0 }, { 2, 4 }).value();
    auto right = make_view(raster, { 4, 0 }, { 2, 4 }).value();
    REQUIRE(algorithm::copy(left, right));
    for (unsigned y = 0; y < 4; ++y) {
        CHECK(raster.pixel({ 4, y }) == original.pixel({ 0, y }));
        CHECK(raster.pixel({ 5, y }) == original.pixel({ 1, y }));
    }
    REQUIRE(algorithm::copy(left, left));
    const auto before_error = raster;
    auto shifted = make_view(raster, { 1, 0 }, { 2, 4 }).value();
    auto result = algorithm::copy(left, shifted);
    REQUIRE_FALSE(result);
    CHECK(result.error().code() == Error::Code::InvalidInput);
    CHECK(std::ranges::equal(raster, before_error));
    auto wrong_size = make_view(raster, { 4, 0 }, { 1, 4 }).value();
    CHECK_FALSE(algorithm::copy(left, wrong_size));
    CHECK(std::ranges::equal(raster, before_error));
}

TEST_CASE("pointwise transforms support different pixel types and exact in-place mappings", "[raster-view]")
{
    auto source = numbered({ 3, 2 });
    radix::Raster<glm::vec2> output(source.size());
    const float factor = 0.5f;
    REQUIRE(algorithm::transform(source, [factor](int value) { return glm::vec2(value * factor, -value); }, output));
    CHECK(output.pixel({ 2, 1 }) == glm::vec2(6.f, -12.f));
    REQUIRE(algorithm::transform(source, [](int value) { return value + 1; }, source));
    CHECK(source.pixel({ 2, 1 }) == 13);

    struct Overloaded {
        int operator()(int&) const { return -100; }
        int operator()(const int& value) const { return value; }
    };
    Raster copied(source.size());
    REQUIRE(algorithm::transform(source, Overloaded {}, copied));
    CHECK(std::ranges::equal(source, copied));

    Raster small(1);
    auto never = [](int) -> int {
        FAIL("invalid arguments must not invoke the callback");
        return 0;
    };
    CHECK_FALSE(algorithm::transform(source, never, small));
    auto overlapping = make_view(source, { 0, 0 }, { 2, 2 }).value();
    auto shifted = make_view(source, { 1, 0 }, { 2, 2 }).value();
    const auto before = source;
    CHECK_FALSE(algorithm::transform(overlapping, never, shifted));
    CHECK(std::ranges::equal(source, before));
}

TEST_CASE("zip transforms mix input types and view kinds in two three and tuple forms", "[raster-view]")
{
    auto first = numbered({ 3, 2 });
    radix::Raster<float> second(first.size(), 0.5f);
    Raster third(first.size(), 3);
    auto clamped = make_clamped_view(third, { -5, -5 }, first.size()).value();
    radix::Raster<double> destination(first.size());
    REQUIRE(algorithm::zip_transform(first, second, [](int a, float b) -> double { return a + 2. * b; }, destination));
    CHECK(destination.pixel({ 2, 1 }) == 13.);
    REQUIRE(algorithm::zip_transform(make_view(first), second, clamped, [](int a, float b, int c) -> double { return a + 2. * b + 3. * c; }, destination));
    CHECK(destination.pixel({ 2, 1 }) == 22.);
    REQUIRE(algorithm::zip_transform(
        std::tuple { make_view(first), make_view(second), clamped, make_view(third) },
        [](int a, float b, int c, int d) -> double { return a + 2. * b + 3. * c + 4. * d; },
        destination));
    CHECK(destination.pixel({ 2, 1 }) == 34.);
    REQUIRE(algorithm::zip_transform(std::tie(first, third, third, third, third), Sum {}, first));
    CHECK(first.pixel({ 2, 1 }) == 24);
    REQUIRE(algorithm::zip_transform(first, first, [](int a, int b) { return a + b; }, first));
    CHECK(first.pixel({ 2, 1 }) == 48);
}

TEST_CASE("zip validates every input before invoking the callable", "[raster-view]")
{
    auto raster = numbered({ 5, 2 });
    auto input = make_view(raster, { 0, 0 }, { 3, 2 }).value();
    auto overlapping = make_view(raster, { 1, 0 }, { 3, 2 }).value();
    Raster independent(input.size(), -1);
    Raster wrong({ 2, 2 }, -1);
    auto never = [](const auto&...) -> int {
        FAIL("zip validation must finish before invoking the callback");
        return 0;
    };
    const auto before = raster;
    CHECK_FALSE(algorithm::zip_transform(independent, independent, overlapping, never, input));
    CHECK(std::ranges::equal(raster, before));
    CHECK_FALSE(algorithm::zip_transform(std::tie(independent, independent, independent, wrong), never, independent));
    CHECK(std::ranges::all_of(independent, [](int value) { return value == -1; }));
    CHECK_FALSE(algorithm::zip_transform(wrong, independent, never, independent));
}

TEST_CASE("clamped overlap uses sampled storage and rejects repeated-pixel aliases", "[raster-view]")
{
    auto raster = numbered({ 5, 2 });
    auto source = make_view(raster, { 0, 0 }, { 1, 2 }).value();
    auto replicated = make_clamped_view(source, { -2, 0 }, { 3, 2 }).value();
    auto disjoint = make_view(raster, { 2, 0 }, { 3, 2 }).value();
    REQUIRE(algorithm::copy(replicated, disjoint));
    CHECK(raster.pixel({ 4, 1 }) == 10);
    auto overlapping = make_view(raster, { 0, 0 }, { 3, 2 }).value();
    const auto before = raster;
    CHECK_FALSE(algorithm::copy(replicated, overlapping));
    CHECK_FALSE(algorithm::transform(replicated, Identity {}, overlapping));
    CHECK(std::ranges::equal(raster, before));
    auto identity = make_clamped_view(raster, { 2, 0 }, { 3, 2 }).value();
    REQUIRE(algorithm::transform(identity, [](int value) { return value + 1; }, disjoint));
    CHECK(raster.pixel({ 4, 1 }) == 11);
}

TEST_CASE("window transforms traverse strided kernels and convert output types", "[raster-view]")
{
    auto raster = numbered({ 8, 6 });
    auto source = make_view(raster, { 1, 1 }, { 5, 4 }).value();
    radix::Raster<double> destination({ 5, 4 }, -1.);
    auto output = make_view(destination, { 1, 1 }, { 3, 2 }).value();
    auto sum = [](const auto& window) -> double {
        static_assert(std::is_const_v<std::remove_reference_t<decltype(window.pixel({ 0, 0 }))>>);
        double result = 0;
        for (unsigned y = 0; y < window.height(); ++y) {
            for (unsigned x = 0; x < window.width(); ++x) {
                result += window.pixel({ x, y });
            }
        }
        return result;
    };
    REQUIRE(algorithm::window_transform(source, 3, sum, output));
    for (unsigned y = 0; y < 2; ++y) {
        for (unsigned x = 0; x < 3; ++x) {
            CHECK(output.pixel({ x, y }) == 9. * (10 * (y + 2) + x + 2));
        }
    }
    CHECK(destination.pixel({ 0, 0 }) == -1.);
    CHECK(destination.pixel({ 4, 3 }) == -1.);
    Raster identity(source.size());
    REQUIRE(algorithm::window_transform(source, 1, Centre {}, identity));
    CHECK(identity.pixel({ 4, 3 }) == 45);
}

TEST_CASE("clamped window kernels retain the original source bounds", "[raster-view]")
{
    auto source = numbered({ 4, 3 });
    auto padded = make_clamped_view(source, { -3, -3 }, source.size() + glm::uvec2(6)).value();
    Raster output(source.size());
    auto sum = [](const auto& window) {
        int result = 0;
        for (unsigned y = 0; y < window.height(); ++y) {
            for (unsigned x = 0; x < window.width(); ++x) {
                result += window.pixel({ x, y });
            }
        }
        return result;
    };
    REQUIRE(algorithm::window_transform(padded, 7, sum, output));
    for (unsigned y = 0; y < source.height(); ++y) {
        for (unsigned x = 0; x < source.width(); ++x) {
            int expected = 0;
            for (int dy = -3; dy <= 3; ++dy) {
                for (int dx = -3; dx <= 3; ++dx) {
                    const auto sx = unsigned(std::clamp(int(x) + dx, 0, 3));
                    const auto sy = unsigned(std::clamp(int(y) + dy, 0, 2));
                    expected += source.pixel({ sx, sy });
                }
            }
            CHECK(output.pixel({ x, y }) == expected);
        }
    }
}

TEST_CASE("window validation precedes processing and rejects all overlapping output", "[raster-view]")
{
    auto source = numbered({ 5, 5 });
    Raster output({ 3, 3 }, -1);
    auto never = [](const auto&) -> int {
        FAIL("invalid windows must not invoke the callback");
        return 0;
    };
    for (const auto kernel : { 0u, 2u, 4u, 7u, (std::numeric_limits<unsigned>::max)() }) {
        CHECK_FALSE(algorithm::window_transform(source, kernel, never, output));
    }
    CHECK_FALSE(algorithm::window_transform(source, 1, never, output));
    CHECK(std::ranges::all_of(output, [](int value) { return value == -1; }));
    auto overlapping = make_view(source, { 1, 1 }, { 3, 3 }).value();
    const auto before = source;
    CHECK_FALSE(algorithm::window_transform(source, 3, never, overlapping));
    CHECK_FALSE(algorithm::window_transform(source, 1, never, source));
    CHECK(std::ranges::equal(source, before));
}

TEST_CASE("matching empty views are pointwise no-ops but cannot supply a kernel", "[raster-view]")
{
    Raster empty({ 0, 3 });
    Raster output({ 0, 3 });
    Raster mismatched({ 0, 4 });
    auto never = [](const auto&...) -> int {
        FAIL("empty views must not invoke the callback");
        return 0;
    };
    REQUIRE(algorithm::copy(empty, output));
    REQUIRE(algorithm::transform(empty, never, output));
    REQUIRE(algorithm::zip_transform(empty, empty, never, output));
    auto clamped = make_clamped_view(empty, { -3, -3 }, { 0, 3 }).value();
    REQUIRE(algorithm::copy(clamped, output));
    CHECK_FALSE(algorithm::copy(empty, mismatched));
    CHECK_FALSE(algorithm::window_transform(empty, 1, never, output));
}

TEST_CASE("destination-free operations infer output types and allocate matching rasters", "[raster-view]")
{
    auto source = numbered({ 4, 3 });
    auto copied = algorithm::copy(source);
    STATIC_REQUIRE(std::same_as<decltype(copied), Expected<Raster>>);
    REQUIRE(copied);
    CHECK(std::ranges::equal(source, *copied));
    copied->pixel({ 0, 0 }) = -1;
    CHECK(source.pixel({ 0, 0 }) == 0);

    auto region = make_view(source, { 1, 1 }, { 2, 2 }).value();
    auto transformed = algorithm::transform(region, [](int value) { return glm::vec2(value, value * 2); });
    STATIC_REQUIRE(std::same_as<decltype(transformed), Expected<radix::Raster<glm::vec2>>>);
    REQUIRE(transformed);
    CHECK(transformed->size() == glm::uvec2(2));
    CHECK(transformed->pixel({ 1, 1 }) == glm::vec2(22, 44));

    auto padded = make_clamped_view(source, { -1, -1 }, source.size() + glm::uvec2(2)).value();
    auto replicated = algorithm::copy(padded);
    REQUIRE(replicated);
    CHECK(replicated->size() == padded.size());
    CHECK(replicated->pixel({ 5, 4 }) == 23);
    auto filtered = algorithm::window_transform(padded, 3, Centre {});
    REQUIRE(filtered);
    CHECK(filtered->size() == source.size());
    CHECK(std::ranges::equal(*filtered, source));
    auto cropped = algorithm::window_transform(source, 3, [](const auto& window) { return double(window.pixel({ 1, 1 })); });
    STATIC_REQUIRE(std::same_as<decltype(cropped), Expected<radix::Raster<double>>>);
    REQUIRE(cropped);
    CHECK(cropped->size() == glm::uvec2(2, 1));
    CHECK(cropped->pixel({ 1, 0 }) == 12.);

    auto two = algorithm::zip_transform(source, source, [](int a, int b) { return double(a + b); });
    STATIC_REQUIRE(std::same_as<decltype(two), Expected<radix::Raster<double>>>);
    REQUIRE(two);
    CHECK(two->pixel({ 3, 2 }) == 46.);
    auto three = algorithm::zip_transform(source, source, source, Sum {});
    REQUIRE(three);
    CHECK(three->pixel({ 3, 2 }) == 69);
    auto five = algorithm::zip_transform(std::tie(source, source, source, source, source), Sum {});
    REQUIRE(five);
    CHECK(five->pixel({ 3, 2 }) == 115);
    auto temporary = algorithm::copy(numbered({ 2, 2 }));
    REQUIRE(temporary);
    CHECK(temporary->pixel({ 1, 1 }) == 11);
}

TEST_CASE("allocating overloads reject invalid geometry and unrepresentable storage before callbacks", "[raster-view]")
{
    Raster source({ 3, 3 }, 1);
    Raster other({ 2, 3 }, 2);
    auto never = [](const auto&...) -> int {
        FAIL("invalid allocation arguments must not invoke callbacks");
        return 0;
    };
    CHECK_FALSE(algorithm::zip_transform(source, other, never));
    CHECK_FALSE(algorithm::zip_transform(source, source, other, never));
    CHECK_FALSE(algorithm::zip_transform(std::tie(source, source, source, other), never));
    CHECK_FALSE(algorithm::window_transform(source, 0, never));
    CHECK_FALSE(algorithm::window_transform(source, 2, never));
    CHECK_FALSE(algorithm::window_transform(source, 5, never));

    const auto maximum = (std::numeric_limits<unsigned>::max)();
    auto huge = make_clamped_view(source, { 0, 0 }, { maximum, maximum }).value();
    auto failed = algorithm::copy(huge);
    REQUIRE_FALSE(failed);
    CHECK(failed.error().code() == Error::Code::ResourceExhausted);
    CHECK_FALSE(algorithm::transform(huge, never));
    CHECK_FALSE(algorithm::zip_transform(huge, huge, never));
    CHECK_FALSE(algorithm::window_transform(huge, 1, never));

    Raster empty({ 0, maximum });
    auto copied = algorithm::copy(empty);
    REQUIRE(copied);
    CHECK(copied->size() == empty.size());
    REQUIRE(algorithm::transform(empty, never));
    REQUIRE(algorithm::zip_transform(empty, empty, never));
}

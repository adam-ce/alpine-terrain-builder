#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <new>
#include <numbers>
#include <numeric>
#include <span>
#include <type_traits>
#include <utility>

#include <glm/glm.hpp>
#include <radix/raster.h>

#include "Error.h"

namespace raster::algorithm {

enum class Interpolation {
    NearestNeighbour,
    Bilinear,
};
enum class Filter {
    Box,
    Lanczos2,
    Lanczos3,
    Lanczos4,
};
enum class ValueMapping {
    Linear,
    SRGBA,
};

namespace detail {

    template <typename T>
    struct PixelTraits {
        static_assert((std::is_integral_v<T> && !std::same_as<T, bool> && sizeof(T) <= 8)
            || std::same_as<T, float> || std::same_as<T, double> || std::same_as<T, long double>);
        using Scalar = T;
        using Working = std::conditional_t<std::same_as<T, float> || (std::is_integral_v<T> && sizeof(T) <= 2),
            float,
            std::conditional_t<std::same_as<T, double> || sizeof(T) <= 4, double, long double>>;
        static constexpr unsigned components = 1;
        static T& component(T& pixel, unsigned) { return pixel; }
        static const T& component(const T& pixel, unsigned) { return pixel; }
    };

    template <glm::length_t N, typename T, glm::qualifier Q>
    struct PixelTraits<glm::vec<N, T, Q>> {
        using Scalar = T;
        using Working = glm::vec<N, typename PixelTraits<T>::Working, Q>;
        static constexpr unsigned components = N;
        static T& component(glm::vec<N, T, Q>& pixel, unsigned i) { return pixel[static_cast<glm::length_t>(i)]; }
        static const T& component(const glm::vec<N, T, Q>& pixel, unsigned i) { return pixel[static_cast<glm::length_t>(i)]; }
    };

} // namespace detail

/// Floating-point pixel representation passed to custom reducers.
template <typename PixelType>
using WorkingPixel = typename detail::PixelTraits<PixelType>::Working;

namespace detail {

    template <typename T>
    using Rasters = std::pair<radix::Raster<T>, radix::Raster<std::uint16_t>>;

    template <typename T>
    using WorkingScalar = typename PixelTraits<WorkingPixel<T>>::Scalar;

    template <typename T>
    Expected<void> validate_mapping(ValueMapping mapping)
    {
        static_assert(!std::same_as<typename PixelTraits<T>::Scalar, long double>, "long double is a working type, not a stored pixel type");
        switch (mapping) {
        case ValueMapping::Linear:
            return {};
        case ValueMapping::SRGBA:
            if constexpr (std::same_as<typename PixelTraits<T>::Scalar, std::uint8_t> && (PixelTraits<T>::components == 3 || PixelTraits<T>::components == 4)) {
                return {};
            }
            return Error::fail(Error::Code::Unsupported, "sRGB mapping requires RGB or RGBA uint8 pixels");
        }
        return Error::fail(Error::Code::InvalidInput, "invalid raster value mapping");
    }

    inline Expected<unsigned> scale_factor(unsigned n_zoom_levels)
    {
        if (n_zoom_levels >= std::numeric_limits<unsigned>::digits) {
            return Error::fail(Error::Code::InvalidInput, "raster zoom-level count exceeds the supported range");
        }
        return 1u << n_zoom_levels;
    }

    inline Expected<unsigned> filter_radius(Filter filter)
    {
        switch (filter) {
        case Filter::Box:
            return 0;
        case Filter::Lanczos2:
            return 2;
        case Filter::Lanczos3:
            return 3;
        case Filter::Lanczos4:
            return 4;
        }
        return Error::fail(Error::Code::InvalidInput, "invalid raster reduction filter");
    }

    inline Expected<unsigned> required_upscaling_halo(unsigned n_zoom_levels, Interpolation interpolation)
    {
        if (auto factor = scale_factor(n_zoom_levels); !factor) {
            return Error::propagate(std::move(factor));
        }
        switch (interpolation) {
        case Interpolation::NearestNeighbour:
            return 0;
        case Interpolation::Bilinear:
            return n_zoom_levels == 0 ? 0u : 1u;
        }
        return Error::fail(Error::Code::InvalidInput, "invalid raster interpolation");
    }

    inline Expected<unsigned> required_downscaling_halo(unsigned n_zoom_levels, Filter filter)
    {
        auto factor = scale_factor(n_zoom_levels);
        if (!factor) {
            return Error::propagate(std::move(factor));
        }
        auto radius = filter_radius(filter);
        if (!radius) {
            return Error::propagate(std::move(radius));
        }
        const auto width = *radius == 0 ? 0 : std::uint64_t(2 * *radius - 1) * (*factor - 1);
        if (width > (std::numeric_limits<unsigned>::max)()) {
            return Error::fail(Error::Code::ResourceExhausted, "required raster halo exceeds representable dimensions");
        }
        return static_cast<unsigned>(width);
    }

    template <typename T>
    Expected<glm::uvec2> validate_input(
        const radix::Raster<T>& data, const radix::Raster<std::uint16_t>& attribution, unsigned halo_width, unsigned required_width, ValueMapping mapping)
    {
        if (auto valid = validate_mapping<T>(mapping); !valid) {
            return Error::propagate(std::move(valid));
        }
        const auto padding = std::uint64_t(halo_width) * 2;
        if (data.size() != attribution.size() || data.width() <= padding || data.height() <= padding) {
            return Error::fail(Error::Code::InvalidInput, "raster dimensions must match and contain a nonempty interior");
        }
        if (halo_width < required_width) {
            return Error::fail(Error::Code::InvalidInput, "insufficient raster halo for the requested scaling operation");
        }
        return data.size() - glm::uvec2(static_cast<unsigned>(padding));
    }

    template <typename T>
    Expected<Rasters<T>> allocate(std::uint64_t width, std::uint64_t height)
    {
        const auto max_dimension = (std::numeric_limits<unsigned>::max)();
        if (width == 0 || height == 0 || width > max_dimension || height > max_dimension || width > (std::numeric_limits<std::size_t>::max)() / height) {
            return Error::fail(Error::Code::ResourceExhausted, "scaled raster dimensions exceed representable limits");
        }
        const auto count = width * height;
        const auto maximum_bytes = static_cast<std::uint64_t>((std::numeric_limits<std::ptrdiff_t>::max)());
        if (count > maximum_bytes / sizeof(T) || count > maximum_bytes / sizeof(std::uint16_t)) {
            return Error::fail(Error::Code::ResourceExhausted, "scaled raster buffers exceed allocation limits");
        }
        const glm::uvec2 size(static_cast<unsigned>(width), static_cast<unsigned>(height));
        try {
            return Rasters<T> { radix::Raster<T>(size), radix::Raster<std::uint16_t>(size) };
        } catch (const std::bad_alloc&) {
            return Error::fail(Error::Code::ResourceExhausted, "allocate scaled raster buffers");
        }
    }

    template <typename T>
    Expected<Rasters<T>> crop(const radix::Raster<T>& data, const radix::Raster<std::uint16_t>& attribution, unsigned halo_width, glm::uvec2 interior)
    {
        auto output = allocate<T>(interior.x, interior.y);
        if (!output) {
            return output;
        }
        for (unsigned y = 0; y < interior.y; ++y) {
            const auto source = std::size_t(y + halo_width) * data.width() + halo_width;
            const auto destination = std::size_t(y) * interior.x;
            std::copy_n(data.buffer().begin() + source, interior.x, output->first.buffer().begin() + destination);
            std::copy_n(attribution.buffer().begin() + source, interior.x, output->second.buffer().begin() + destination);
        }
        return output;
    }

    template <typename T>
    WorkingPixel<T> decode(const T& pixel, ValueMapping mapping)
    {
        using W = WorkingScalar<T>;
        WorkingPixel<T> result {};
        for (unsigned i = 0; i < PixelTraits<T>::components; ++i) {
            W value = static_cast<W>(PixelTraits<T>::component(pixel, i));
            if (mapping == ValueMapping::SRGBA) {
                value /= W(255);
                if (i < 3) {
                    value = value <= W(0.04045) ? value / W(12.92) : std::pow((value + W(0.055)) / W(1.055), W(2.4));
                }
            }
            PixelTraits<WorkingPixel<T>>::component(result, i) = value;
        }
        return result;
    }

    template <typename T>
    Expected<T> encode(const WorkingPixel<T>& pixel, ValueMapping mapping)
    {
        using S = typename PixelTraits<T>::Scalar;
        using W = WorkingScalar<T>;
        T result {};
        for (unsigned i = 0; i < PixelTraits<T>::components; ++i) {
            W value = PixelTraits<WorkingPixel<T>>::component(pixel, i);
            if constexpr (std::is_integral_v<S>) {
                if (!std::isfinite(value)) {
                    return Error::fail(Error::Code::InvalidInput, "nonfinite raster result cannot be stored as an integer");
                }
                if (mapping == ValueMapping::SRGBA) {
                    value = std::clamp(value, W(0), W(1));
                    if (i < 3) {
                        value = value <= W(0.0031308) ? W(12.92) * value : W(1.055) * std::pow(value, W(1) / W(2.4)) - W(0.055);
                    }
                    value *= W(255);
                }
                value = std::round(value);
                // Compare before conversion: a floating representation of an integer
                // maximum can itself round up to the first unrepresentable integer.
                if (value <= static_cast<W>((std::numeric_limits<S>::lowest)())) {
                    PixelTraits<T>::component(result, i) = (std::numeric_limits<S>::lowest)();
                } else if (value >= static_cast<W>((std::numeric_limits<S>::max)())) {
                    PixelTraits<T>::component(result, i) = (std::numeric_limits<S>::max)();
                } else {
                    PixelTraits<T>::component(result, i) = static_cast<S>(value);
                }
            } else {
                PixelTraits<T>::component(result, i) = value;
            }
        }
        return result;
    }

    enum class Reduction {
        Minimum,
        Maximum,
        Median,
    };

    template <Reduction operation, typename T>
    T reduce_components(std::span<const T> samples)
    {
        assert(!samples.empty() && samples.size() <= 4);
        T result {};
        using S = typename PixelTraits<T>::Scalar;
        for (unsigned component = 0; component < PixelTraits<T>::components; ++component) {
            std::array<S, 4> values {};
            bool has_nan = false;
            for (std::size_t i = 0; i < samples.size(); ++i) {
                values[i] = PixelTraits<T>::component(samples[i], component);
                has_nan = has_nan || std::isnan(values[i]);
            }
            S value {};
            if (has_nan) {
                value = std::numeric_limits<S>::quiet_NaN();
            } else {
                auto end = values.begin() + samples.size();
                if constexpr (operation == Reduction::Minimum) {
                    value = *std::min_element(values.begin(), end);
                } else if constexpr (operation == Reduction::Maximum) {
                    value = *std::max_element(values.begin(), end);
                } else {
                    std::sort(values.begin(), end);
                    value = values[samples.size() / 2];
                    if (samples.size() % 2 == 0) {
                        const auto lower = values[samples.size() / 2 - 1];
                        value = std::isfinite(lower) && std::isfinite(value) ? std::midpoint(lower, value) : (lower + value) / S(2);
                    }
                }
            }
            PixelTraits<T>::component(result, component) = value;
        }
        return result;
    }

    struct LocalBlock {
        std::array<glm::uvec2, 4> positions {};
        unsigned count = 0;
        std::uint16_t attribution = 0;
    };

    inline LocalBlock local_block(const radix::Raster<std::uint16_t>& attribution, glm::uvec2 top_left)
    {
        LocalBlock result;
        std::array<std::uint16_t, 4> ids {};
        for (unsigned y = 0; y < 2; ++y) {
            for (unsigned x = 0; x < 2; ++x) {
                const auto position = top_left + glm::uvec2(x, y);
                const auto id = attribution.pixel(position);
                if (id != 0) {
                    ids[result.count] = id;
                    result.positions[result.count++] = position;
                }
            }
        }
        if (result.count != 0) {
            result.attribution = ids[0];
            for (unsigned i = 0; i < result.count; ++i) {
                if (std::count(ids.begin(), ids.begin() + result.count, ids[i]) >= 2) {
                    result.attribution = ids[i];
                    break;
                }
            }
        }
        return result;
    }

    template <typename T>
    struct Kernel {
        using W = WorkingScalar<T>;
        std::array<W, 16> weights {};
        int first = 0;
        unsigned count = 2;

        explicit Kernel(unsigned radius)
        {
            if (radius == 0) {
                weights[0] = weights[1] = W(1);
                return;
            }
            first = 1 - 2 * static_cast<int>(radius);
            count = 4 * radius;
            const auto sinc = [](W x) {
                const W angle = std::numbers::pi_v<W> * x;
                return x == W(0) ? W(1) : std::sin(angle) / angle;
            };
            for (unsigned i = 0; i < count; ++i) {
                const W offset = W(first + static_cast<int>(i)) - W(0.5);
                weights[i] = sinc(offset / W(2)) * sinc(offset / W(2 * radius));
            }
        }

        Expected<T> operator()(const radix::Raster<T>& data,
            const radix::Raster<std::uint16_t>& attribution,
            glm::uvec2 top_left,
            const LocalBlock& local,
            ValueMapping mapping) const
        {
            const auto origin_x = std::int64_t(top_left.x) + first;
            const auto origin_y = std::int64_t(top_left.y) + first;
            W sum = 0;
            W absolute_sum = 0;
            for (unsigned y = 0; y < count; ++y) {
                for (unsigned x = 0; x < count; ++x) {
                    const glm::uvec2 position(static_cast<unsigned>(origin_x + x), static_cast<unsigned>(origin_y + y));
                    if (attribution.pixel(position) == 0) {
                        continue;
                    }
                    const W weight = weights[x] * weights[y];
                    sum += weight;
                    absolute_sum += std::abs(weight);
                }
            }
            constexpr W cancellation_tolerance = W(32) * std::numeric_limits<W>::epsilon();
            if (std::abs(sum) <= cancellation_tolerance * absolute_sum) {
                return data.pixel(local.positions[0]);
            }
            WorkingPixel<T> value {};
            for (unsigned y = 0; y < count; ++y) {
                for (unsigned x = 0; x < count; ++x) {
                    const W weight = weights[x] * weights[y];
                    const glm::uvec2 position(static_cast<unsigned>(origin_x + x), static_cast<unsigned>(origin_y + y));
                    if (weight != W(0) && attribution.pixel(position) != 0) {
                        value += decode(data.pixel(position), mapping) * (weight / sum);
                    }
                }
            }
            return encode<T>(value, mapping);
        }
    };

    // The callback supplies a single reduction step; this driver owns validity,
    // attribution, intermediate storage, and the shrinking filter halos.
    template <typename T, typename PixelReducer>
    Expected<Rasters<T>> reduce_levels(const radix::Raster<T>& data,
        const radix::Raster<std::uint16_t>& attribution,
        unsigned halo_width,
        unsigned n_zoom_levels,
        unsigned base_halo,
        ValueMapping mapping,
        PixelReducer&& reducer)
    {
        auto factor = scale_factor(n_zoom_levels);
        if (!factor) {
            return Error::propagate(std::move(factor));
        }
        const auto required = std::uint64_t(base_halo) * (*factor - 1);
        if (required > (std::numeric_limits<unsigned>::max)()) {
            return Error::fail(Error::Code::ResourceExhausted, "required raster halo exceeds representable dimensions");
        }
        auto interior = validate_input(data, attribution, halo_width, static_cast<unsigned>(required), mapping);
        if (!interior) {
            return Error::propagate(std::move(interior));
        }
        if (interior->x % *factor != 0 || interior->y % *factor != 0) {
            return Error::fail(Error::Code::InvalidInput, "raster interior dimensions must be divisible by the reduction factor");
        }
        if (n_zoom_levels == 0) {
            return crop(data, attribution, halo_width, *interior);
        }
        const auto* current_data = &data;
        const auto* current_attribution = &attribution;
        Rasters<T> intermediate;
        for (unsigned remaining_factor = *factor / 2;; remaining_factor /= 2) {
            *interior /= 2u;
            const unsigned output_halo = base_halo * (remaining_factor - 1);
            auto output = allocate<T>(std::uint64_t(interior->x) + 2ull * output_halo, std::uint64_t(interior->y) + 2ull * output_halo);
            if (!output) {
                return output;
            }
            for (unsigned y = 0; y < output->first.height(); ++y) {
                for (unsigned x = 0; x < output->first.width(); ++x) {
                    const glm::uvec2 top_left(static_cast<unsigned>(2 * (std::int64_t(x) - output_halo) + halo_width),
                        static_cast<unsigned>(2 * (std::int64_t(y) - output_halo) + halo_width));
                    const auto local = local_block(*current_attribution, top_left);
                    output->second.pixel({ x, y }) = local.attribution;
                    if (local.count == 0) {
                        output->first.pixel({ x, y }) = current_data->pixel(top_left + glm::uvec2(1));
                    } else {
                        auto pixel = reducer(*current_data, *current_attribution, top_left, local, mapping);
                        if (!pixel) {
                            return Error::propagate(std::move(pixel), "reduce raster pixel");
                        }
                        output->first.pixel({ x, y }) = *pixel;
                    }
                }
            }
            intermediate = std::move(*output);
            if (remaining_factor == 1) {
                return intermediate;
            }
            current_data = &intermediate.first;
            current_attribution = &intermediate.second;
            halo_width = output_halo;
        }
    }

    template <typename T>
    Expected<Rasters<T>> upscale(const radix::Raster<T>& data,
        const radix::Raster<std::uint16_t>& attribution,
        unsigned halo_width,
        unsigned n_zoom_levels,
        Interpolation interpolation,
        ValueMapping mapping)
    {
        auto required = required_upscaling_halo(n_zoom_levels, interpolation);
        if (!required) {
            return Error::propagate(std::move(required));
        }
        auto interior = validate_input(data, attribution, halo_width, *required, mapping);
        if (!interior) {
            return Error::propagate(std::move(interior));
        }
        if (n_zoom_levels == 0) {
            return crop(data, attribution, halo_width, *interior);
        }
        const unsigned factor = *scale_factor(n_zoom_levels);
        auto output = allocate<T>(std::uint64_t(interior->x) * factor, std::uint64_t(interior->y) * factor);
        if (!output) {
            return output;
        }
        using W = WorkingScalar<T>;
        for (unsigned y = 0; y < output->first.height(); ++y) {
            for (unsigned x = 0; x < output->first.width(); ++x) {
                const glm::uvec2 nearest(x / factor + halo_width, y / factor + halo_width);
                const auto id = attribution.pixel(nearest);
                output->second.pixel({ x, y }) = id;
                if (interpolation == Interpolation::NearestNeighbour || id == 0) {
                    output->first.pixel({ x, y }) = data.pixel(nearest);
                    continue;
                }
                // Compute the phase separately from the integer index so large
                // raster coordinates do not consume the working pixel's precision.
                const double phase_x = (double(x % factor) + 0.5) / factor - 0.5;
                const double phase_y = (double(y % factor) + 0.5) / factor - 0.5;
                const glm::uvec2 origin(nearest.x - (phase_x < 0 ? 1u : 0u), nearest.y - (phase_y < 0 ? 1u : 0u));
                const W fraction_x = W(phase_x < 0 ? phase_x + 1 : phase_x);
                const W fraction_y = W(phase_y < 0 ? phase_y + 1 : phase_y);
                const std::array<W, 2> weights_x { W(1) - fraction_x, fraction_x };
                const std::array<W, 2> weights_y { W(1) - fraction_y, fraction_y };
                std::array<W, 4> weights {};
                W sum = 0;
                for (unsigned dy = 0; dy < 2; ++dy) {
                    for (unsigned dx = 0; dx < 2; ++dx) {
                        if (attribution.pixel(origin + glm::uvec2(dx, dy)) != 0) {
                            weights[2 * dy + dx] = weights_x[dx] * weights_y[dy];
                            sum += weights[2 * dy + dx];
                        }
                    }
                }
                WorkingPixel<T> value {};
                for (unsigned dy = 0; dy < 2; ++dy) {
                    for (unsigned dx = 0; dx < 2; ++dx) {
                        const W weight = weights[2 * dy + dx];
                        if (weight != W(0)) {
                            value += decode(data.pixel(origin + glm::uvec2(dx, dy)), mapping) * (weight / sum);
                        }
                    }
                }
                auto pixel = encode<T>(value, mapping);
                if (!pixel) {
                    return Error::propagate(std::move(pixel), "interpolate raster pixel");
                }
                output->first.pixel({ x, y }) = *pixel;
            }
        }
        return output;
    }

    template <typename T>
    Expected<Rasters<T>> downscale(const radix::Raster<T>& data,
        const radix::Raster<std::uint16_t>& attribution,
        unsigned halo_width,
        unsigned n_zoom_levels,
        Filter filter,
        ValueMapping mapping)
    {
        auto radius = filter_radius(filter);
        if (!radius) {
            return Error::propagate(std::move(radius));
        }
        return reduce_levels(data, attribution, halo_width, n_zoom_levels, *radius == 0 ? 0u : 2 * *radius - 1, mapping, Kernel<T>(*radius));
    }

} // namespace detail

/// Component-wise reducers for the nonempty, at-most-four-sample working spans.
struct Min {
    template <typename T>
    T operator()(std::span<const T> samples) const
    {
        return detail::reduce_components<detail::Reduction::Minimum>(samples);
    }
};

struct Max {
    template <typename T>
    T operator()(std::span<const T> samples) const
    {
        return detail::reduce_components<detail::Reduction::Maximum>(samples);
    }
};

struct Median {
    template <typename T>
    T operator()(std::span<const T> samples) const
    {
        return detail::reduce_components<detail::Reduction::Median>(samples);
    }
};

/// Positive levels upscale, negative levels downscale, and zero only crops.
inline Expected<unsigned> required_halo(int n_zoom_levels, Interpolation interpolation, Filter filter)
{
    if (auto valid = detail::required_upscaling_halo(0, interpolation); !valid) {
        return Error::propagate(std::move(valid));
    }
    if (auto valid = detail::filter_radius(filter); !valid) {
        return Error::propagate(std::move(valid));
    }
    if (n_zoom_levels < 0) {
        return detail::required_downscaling_halo(static_cast<unsigned>(-std::int64_t(n_zoom_levels)), filter);
    }
    return detail::required_upscaling_halo(static_cast<unsigned>(n_zoom_levels), interpolation);
}

/// Returns data first and attribution second, with the complete halo removed.
template <typename PixelType>
Expected<std::pair<radix::Raster<PixelType>, radix::Raster<std::uint16_t>>> scale(const radix::Raster<PixelType>& data,
    const radix::Raster<std::uint16_t>& source_attribution,
    unsigned halo_width,
    int n_zoom_levels,
    Interpolation interpolation,
    Filter filter,
    ValueMapping value_mapping)
{
    if (auto required = required_halo(n_zoom_levels, interpolation, filter); !required) {
        return Error::propagate(std::move(required));
    }
    if (n_zoom_levels < 0) {
        return detail::downscale(data, source_attribution, halo_width, static_cast<unsigned>(-std::int64_t(n_zoom_levels)), filter, value_mapping);
    }
    return detail::upscale(data, source_attribution, halo_width, static_cast<unsigned>(n_zoom_levels), interpolation, value_mapping);
}

/// Repeated 2x2 reduction. The same callable receives valid, linear samples at
/// every stage. Zero levels crop without invoking it; empty blocks bypass it.
template <typename PixelType, typename Reducer>
requires std::invocable<Reducer&, std::span<const WorkingPixel<PixelType>>>
    && std::same_as<std::invoke_result_t<Reducer&, std::span<const WorkingPixel<PixelType>>>, WorkingPixel<PixelType>>
Expected<std::pair<radix::Raster<PixelType>, radix::Raster<std::uint16_t>>> reduce(const radix::Raster<PixelType>& data,
    const radix::Raster<std::uint16_t>& source_attribution,
    unsigned halo_width,
    unsigned n_zoom_levels,
    ValueMapping value_mapping,
    Reducer&& reducer)
{
    const auto apply_reducer = [&reducer](const radix::Raster<PixelType>& input,
                                   const radix::Raster<std::uint16_t>&,
                                   glm::uvec2,
                                   const detail::LocalBlock& local,
                                   ValueMapping mapping) -> Expected<PixelType> {
        std::array<WorkingPixel<PixelType>, 4> samples {};
        for (unsigned i = 0; i < local.count; ++i) {
            samples[i] = detail::decode(input.pixel(local.positions[i]), mapping);
        }
        return detail::encode<PixelType>(std::invoke(reducer, std::span<const WorkingPixel<PixelType>>(samples.data(), local.count)), mapping);
    };
    return detail::reduce_levels(data, source_attribution, halo_width, n_zoom_levels, 0, value_mapping, apply_reducer);
}

} // namespace raster::algorithm

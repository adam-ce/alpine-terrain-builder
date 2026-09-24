#pragma once

#include <array>
#include <numbers>

#include "conversion.h"
#include "scaling_detail.h"

namespace raster::algorithm {
namespace detail {
    template <typename InputView, typename T, typename Conversion>
    Expected<void> upscale(const InputView& source,
        unsigned halo_width,
        const ScalingGeometry& geometry,
        Interpolation interpolation,
        const Conversion& conversion,
        const View<T>& destination,
        glm::uvec2 output_offset = {})
    {
        using W = DecodedPixel<Conversion, T>;
        using S = typename PixelTraits<W>::Scalar;
        const unsigned factor = geometry.factor;
        for (unsigned y = 0; y < destination.height(); ++y) {
            for (unsigned x = 0; x < destination.width(); ++x) {
                const glm::uvec2 position = output_offset + glm::uvec2(x, y);
                const glm::uvec2 nearest = position / factor + glm::uvec2(halo_width);
                if (interpolation == Interpolation::NearestNeighbour) {
                    std::memcpy(std::addressof(destination.pixel({ x, y })), std::addressof(source.pixel(nearest)), sizeof(T));
                    continue;
                }
                // Keep integer coordinates out of the working pixel's phase precision.
                const S phase_x = (S(position.x % factor) + S(0.5)) / S(factor) - S(0.5);
                const S phase_y = (S(position.y % factor) + S(0.5)) / S(factor) - S(0.5);
                const glm::uvec2 origin(nearest.x - (phase_x < 0 ? 1u : 0u), nearest.y - (phase_y < 0 ? 1u : 0u));
                const S fraction_x = S(phase_x < 0 ? phase_x + 1 : phase_x);
                const S fraction_y = S(phase_y < 0 ? phase_y + 1 : phase_y);
                const std::array<S, 2> weights_x { S(1) - fraction_x, fraction_x };
                const std::array<S, 2> weights_y { S(1) - fraction_y, fraction_y };
                W value {};
                for (unsigned dy = 0; dy < 2; ++dy) {
                    for (unsigned dx = 0; dx < 2; ++dx) {
                        value += std::invoke(std::get<0>(conversion), source.pixel(origin + glm::uvec2(dx, dy))) * (weights_x[dx] * weights_y[dy]);
                    }
                }
                destination.pixel({ x, y }) = std::invoke(std::get<1>(conversion), std::as_const(value));
            }
        }
        return {};
    }

    template <typename InputView, typename T, typename Conversion>
    Expected<void> downscale(const InputView& source,
        unsigned halo_width,
        const ScalingGeometry& geometry,
        unsigned radius,
        const Conversion& conversion,
        const View<T>& destination)
    {
        using W = DecodedPixel<Conversion, T>;
        using S = typename PixelTraits<W>::Scalar;
        std::array<S, 16> weights {};
        const unsigned count = radius == 0 ? 2 : 4 * radius;
        S normalization = 0;
        const auto sinc = [](S x) {
            const S angle = std::numbers::pi_v<S> * x;
            return x == S(0) ? S(1) : std::sin(angle) / angle;
        };
        for (unsigned i = 0; i < count; ++i) {
            const S offset = S(i) - S(count - 1) / S(2);
            weights[i] = radius == 0 ? S(1) : sinc(offset / S(2)) * sinc(offset / S(2 * radius));
            normalization += weights[i];
        }
        for (unsigned i = 0; i < count; ++i) {
            weights[i] /= normalization;
        }
        const auto filter = [&](const auto& input, const auto& output) -> Expected<void> {
            auto horizontal = window_transform(input, { count, 1 }, { 2, 1 }, [&](const auto& window) -> W {
                W value {};
                for (unsigned x = 0; x < count; ++x) {
                    value += std::invoke(std::get<0>(conversion), window.pixel({ x, 0 })) * weights[x];
                }
                return value;
            });
            if (!horizontal) {
                return Error::propagate(std::move(horizontal));
            }
            return window_transform(
                *horizontal,
                { 1, count },
                { 1, 2 },
                [&](const auto& window) -> T {
                    W value {};
                    for (unsigned y = 0; y < count; ++y) {
                        value += window.pixel({ 0, y }) * weights[y];
                    }
                    return std::invoke(std::get<1>(conversion), std::as_const(value));
                },
                output);
        };
        return reduce_levels(source, halo_width, geometry, radius == 0 ? 0 : 2 * radius - 1, filter, destination);
    }

    template <typename InputView, typename T, typename Conversion>
    Expected<void> scale_views(const InputView& source,
        unsigned halo_width,
        int levels,
        Interpolation interpolation,
        Filter filter,
        const Conversion& conversion,
        const View<T>& destination)
    {
        auto geometry = scale_geometry(source.size(), halo_width, levels, interpolation, filter);
        if (!geometry) {
            return Error::propagate(std::move(geometry));
        }
        if (auto valid = validate_scaling_destination(source, halo_width, *geometry, destination); !valid) {
            return valid;
        }
        if (levels == 0) {
            return copy_views(*raster::make_view(source, glm::uvec2(halo_width), geometry->interior), destination);
        }
        if (levels < 0) {
            return downscale(source, halo_width, *geometry, *filter_radius(filter), conversion, destination);
        }
        return upscale(source, halo_width, *geometry, interpolation, conversion, destination);
    }
} // namespace detail

/// All supplied pixels participate. Decoder/intermediate values must be finite and encodable.
template <detail::ViewSource Source, typename Conversion, detail::WritableViewDestination Destination>
requires detail::NumericPixel<detail::SourcePixel<Source>> && detail::ScalingConversion<Conversion, detail::SourcePixel<Source>>
    && std::same_as<detail::SourcePixel<Source>, detail::SourcePixel<Destination>>
[[nodiscard]] Expected<void> scale(Source&& source,
    unsigned halo_width,
    int n_zoom_levels,
    Interpolation interpolation,
    Filter filter,
    const Conversion& conversion,
    Destination&& destination)
{
    return detail::scale_views(
        detail::read_only_view(raster::make_view(source)), halo_width, n_zoom_levels, interpolation, filter, conversion, raster::make_view(destination));
}

template <detail::ViewSource Source, typename Conversion>
requires detail::NumericPixel<detail::SourcePixel<Source>> && detail::ScalingConversion<Conversion, detail::SourcePixel<Source>>
[[nodiscard]] Expected<radix::Raster<detail::SourcePixel<Source>>> scale(
    Source&& source, unsigned halo_width, int n_zoom_levels, Interpolation interpolation, Filter filter, const Conversion& conversion)
{
    const auto input = detail::read_only_view(raster::make_view(source));
    auto geometry = detail::scale_geometry(input.size(), halo_width, n_zoom_levels, interpolation, filter);
    if (!geometry) {
        return Error::propagate(std::move(geometry));
    }
    return detail::produce_raster<detail::SourcePixel<Source>>(geometry->output,
        [&](const auto& destination) { return detail::scale_views(input, halo_width, n_zoom_levels, interpolation, filter, conversion, destination); });
}

template <detail::ViewSource Source, detail::WritableViewDestination Destination>
requires detail::NumericPixel<detail::SourcePixel<Source>> && std::same_as<detail::SourcePixel<Source>, detail::SourcePixel<Destination>>
[[nodiscard]] Expected<void> scale(
    Source&& source, unsigned halo_width, int n_zoom_levels, Interpolation interpolation, Filter filter, Destination&& destination)
{
    return scale(std::forward<Source>(source),
        halo_width,
        n_zoom_levels,
        interpolation,
        filter,
        linear_conversion<detail::SourcePixel<Source>>(),
        std::forward<Destination>(destination));
}

template <detail::ViewSource Source>
requires detail::NumericPixel<detail::SourcePixel<Source>>
[[nodiscard]] Expected<radix::Raster<detail::SourcePixel<Source>>> scale(
    Source&& source, unsigned halo_width, int n_zoom_levels, Interpolation interpolation, Filter filter)
{
    return scale(std::forward<Source>(source), halo_width, n_zoom_levels, interpolation, filter, linear_conversion<detail::SourcePixel<Source>>());
}
/// Select a window of the conceptual full output; the destination determines its size.
template <detail::ViewSource Source, typename Conversion, detail::WritableViewDestination Destination>
requires detail::NumericPixel<detail::SourcePixel<Source>> && detail::ScalingConversion<Conversion, detail::SourcePixel<Source>>
    && std::same_as<detail::SourcePixel<Source>, detail::SourcePixel<Destination>>
[[nodiscard]] Expected<void> scale(Source&& source,
    unsigned halo_width,
    int levels,
    Interpolation interpolation,
    Filter filter,
    glm::uvec2 output_offset,
    const Conversion& conversion,
    Destination&& destination)
{
    const auto input = detail::read_only_view(raster::make_view(source));
    const auto output = raster::make_view(destination);
    auto geometry = detail::window_geometry(input.size(), halo_width, levels, interpolation, filter, output_offset, output.size());
    if (!geometry)
        return Error::propagate(std::move(geometry));
    if (levels == 0) {
        return detail::copy_views(*raster::make_view(input, output_offset + glm::uvec2(halo_width), output.size()), output);
    }
    if (auto valid = detail::validate_view_overlap(input, output, false); !valid)
        return valid;
    if (levels > 0)
        return detail::upscale(input, halo_width, *geometry, interpolation, conversion, output, output_offset);
    // An aligned source crop with the complete cumulative kernel support has
    // exactly the same stage alignment and rounding as full-output reduction.
    const unsigned support = *required_halo(levels, interpolation, filter);
    auto region = raster::make_view(
        input, output_offset * geometry->factor + glm::uvec2(halo_width - support), output.size() * geometry->factor + glm::uvec2(2 * support));
    if (!region)
        return Error::propagate(std::move(region));
    return detail::scale_views(*region, support, levels, interpolation, filter, conversion, output);
}

template <detail::ViewSource Source, typename Conversion>
requires detail::NumericPixel<detail::SourcePixel<Source>> && detail::ScalingConversion<Conversion, detail::SourcePixel<Source>>
[[nodiscard]] Expected<radix::Raster<detail::SourcePixel<Source>>> scale(Source&& source,
    unsigned halo_width,
    int levels,
    Interpolation interpolation,
    Filter filter,
    glm::uvec2 output_offset,
    glm::uvec2 output_size,
    const Conversion& conversion)
{
    auto geometry = detail::window_geometry(source.size(), halo_width, levels, interpolation, filter, output_offset, output_size);
    if (!geometry)
        return Error::propagate(std::move(geometry));
    return detail::produce_raster<detail::SourcePixel<Source>>(
        output_size, [&](const auto& destination) { return scale(source, halo_width, levels, interpolation, filter, output_offset, conversion, destination); });
}

template <detail::ViewSource Source, detail::WritableViewDestination Destination>
requires detail::NumericPixel<detail::SourcePixel<Source>> && std::same_as<detail::SourcePixel<Source>, detail::SourcePixel<Destination>>
[[nodiscard]] Expected<void> scale(
    Source&& source, unsigned halo_width, int levels, Interpolation interpolation, Filter filter, glm::uvec2 output_offset, Destination&& destination)
{
    return scale(source, halo_width, levels, interpolation, filter, output_offset, linear_conversion<detail::SourcePixel<Source>>(), destination);
}

template <detail::ViewSource Source>
requires detail::NumericPixel<detail::SourcePixel<Source>>
[[nodiscard]] Expected<radix::Raster<detail::SourcePixel<Source>>> scale(
    Source&& source, unsigned halo_width, int levels, Interpolation interpolation, Filter filter, glm::uvec2 output_offset, glm::uvec2 output_size)
{
    return scale(source, halo_width, levels, interpolation, filter, output_offset, output_size, linear_conversion<detail::SourcePixel<Source>>());
}
} // namespace raster::algorithm

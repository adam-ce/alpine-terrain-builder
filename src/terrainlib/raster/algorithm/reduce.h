/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
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

#pragma once

#include "reducers.h"
#include "scaling_detail.h"

namespace raster::algorithm {
namespace detail {
    template <typename Conversion, typename Reducer, typename T>
    concept PixelReduction = NumericPixel<T> && PixelConversion<Conversion, T>
        && PixelFunction<Reducer, DecodedPixel<Conversion, T>, std::span<const DecodedPixel<Conversion, T>>>;

    template <typename InputView, typename T, typename Conversion, typename Reducer>
    Expected<void> reduce_views(
        const InputView& source, unsigned halo_width, unsigned levels, const Conversion& conversion, const Reducer& reducer, const View<T>& destination)
    {
        auto geometry = scaling_geometry(source.size(), halo_width, levels, false, 0);
        if (!geometry) {
            return Error::propagate(std::move(geometry));
        }
        if (auto valid = validate_scaling_destination(source, halo_width, *geometry, destination); !valid) {
            return valid;
        }
        using W = DecodedPixel<Conversion, T>;
        const auto step = [&](const auto& input, const auto& output) {
            return window_transform(
                input,
                { 2, 2 },
                { 2, 2 },
                [&](const auto& window) -> T {
                    const std::array<W, 4> samples { std::invoke(std::get<0>(conversion), window.pixel({ 0, 0 })),
                        std::invoke(std::get<0>(conversion), window.pixel({ 1, 0 })),
                        std::invoke(std::get<0>(conversion), window.pixel({ 0, 1 })),
                        std::invoke(std::get<0>(conversion), window.pixel({ 1, 1 })) };
                    const W value = std::invoke(reducer, std::span<const W>(samples));
                    return std::invoke(std::get<1>(conversion), value);
                },
                output);
        };
        return reduce_levels(source, halo_width, *geometry, 0, step, destination);
    }
} // namespace detail

/// Repeated 2x2 reduction with four decoded row-major samples; zero levels only crop.
template <detail::ViewSource Source, typename Conversion, typename Reducer, detail::WritableViewDestination Destination>
requires detail::PixelReduction<Conversion, Reducer, detail::SourcePixel<Source>> && std::same_as<detail::SourcePixel<Source>, detail::SourcePixel<Destination>>
[[nodiscard]] Expected<void> reduce(
    Source&& source, unsigned halo_width, unsigned n_zoom_levels, const Conversion& conversion, const Reducer& reducer, Destination&& destination)
{
    return detail::reduce_views(
        detail::read_only_view(raster::make_view(source)), halo_width, n_zoom_levels, conversion, reducer, raster::make_view(destination));
}

template <detail::ViewSource Source, typename Conversion, typename Reducer>
requires detail::PixelReduction<Conversion, Reducer, detail::SourcePixel<Source>>
[[nodiscard]] Expected<radix::Raster<detail::SourcePixel<Source>>> reduce(
    Source&& source, unsigned halo_width, unsigned n_zoom_levels, const Conversion& conversion, const Reducer& reducer)
{
    const auto input = detail::read_only_view(raster::make_view(source));
    auto geometry = detail::scaling_geometry(input.size(), halo_width, n_zoom_levels, false, 0);
    if (!geometry) {
        return Error::propagate(std::move(geometry));
    }
    return detail::produce_raster<detail::SourcePixel<Source>>(
        geometry->output, [&](const auto& destination) { return detail::reduce_views(input, halo_width, n_zoom_levels, conversion, reducer, destination); });
}

template <detail::ViewSource Source, typename Reducer, detail::WritableViewDestination Destination>
requires detail::PixelReduction<decltype(identity_conversion()), Reducer, detail::SourcePixel<Source>>
    && std::same_as<detail::SourcePixel<Source>, detail::SourcePixel<Destination>>
[[nodiscard]] Expected<void> reduce(Source&& source, unsigned halo_width, unsigned n_zoom_levels, const Reducer& reducer, Destination&& destination)
{
    return reduce(std::forward<Source>(source), halo_width, n_zoom_levels, identity_conversion(), reducer, std::forward<Destination>(destination));
}

template <detail::ViewSource Source, typename Reducer>
requires detail::PixelReduction<decltype(identity_conversion()), Reducer, detail::SourcePixel<Source>>
[[nodiscard]] Expected<radix::Raster<detail::SourcePixel<Source>>> reduce(Source&& source, unsigned halo_width, unsigned n_zoom_levels, const Reducer& reducer)
{
    return reduce(std::forward<Source>(source), halo_width, n_zoom_levels, identity_conversion(), reducer);
}
} // namespace raster::algorithm

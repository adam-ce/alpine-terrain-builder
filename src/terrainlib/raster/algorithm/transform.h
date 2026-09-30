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

#include "detail.h"

namespace raster::algorithm {

namespace detail {
    template <typename InputView, typename Function, typename T>
    requires(!std::is_const_v<T>) && PixelFunction<Function, T, typename InputView::value_type>
    Expected<void> transform_views(const InputView& source, const Function& function, const View<T>& destination)
    {
        if (auto valid = validate_pointwise_views(source, destination); !valid) {
            return valid;
        }
        if (destination.width() == 0 || destination.height() == 0) {
            return {};
        }
        for (unsigned y = 0; y < destination.height(); ++y) {
            for (unsigned x = 0; x < destination.width(); ++x) {
                destination.pixel({ x, y }) = std::invoke(function, std::as_const(source.pixel({ x, y })));
            }
        }
        return {};
    }
} // namespace detail

/// The callable receives a read-only pixel and returns exactly the output type.
template <detail::ViewSource Source, typename Function, detail::WritableViewDestination Destination>
requires detail::PixelFunction<Function, detail::SourcePixel<Destination>, detail::SourcePixel<Source>>
[[nodiscard]] Expected<void> transform(Source&& source, const Function& function, Destination&& destination)
{
    return detail::transform_views(raster::make_view(source), function, raster::make_view(destination));
}

template <detail::ViewSource Source, typename Function>
requires detail::RasterFunction<Function, detail::SourcePixel<Source>>
[[nodiscard]] auto transform(Source&& source, const Function& function)
    -> Expected<radix::Raster<detail::FunctionOutput<Function, detail::SourcePixel<Source>>>>
{
    const auto input = raster::make_view(source);
    return detail::produce_raster<detail::FunctionOutput<Function, detail::SourcePixel<Source>>>(
        input.size(), [&](const auto& destination) { return detail::transform_views(input, function, destination); });
}

} // namespace raster::algorithm

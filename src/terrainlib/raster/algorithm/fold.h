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

/// Accumulate read-only pixels in row order. Empty sources return the initial state.
template <detail::ViewSource Source, std::movable State, typename Function>
requires std::invocable<const Function&, State, const detail::SourcePixel<Source>&>
    && std::same_as<std::invoke_result_t<const Function&, State, const detail::SourcePixel<Source>&>, State>
[[nodiscard]] State fold(Source&& source, State initial, const Function& function)
{
    const auto input = raster::make_view(source);
    for (unsigned y = 0; y < input.height(); ++y) {
        for (unsigned x = 0; x < input.width(); ++x) {
            initial = std::invoke(function, std::move(initial), std::as_const(input.pixel({ x, y })));
        }
    }
    return initial;
}

} // namespace raster::algorithm

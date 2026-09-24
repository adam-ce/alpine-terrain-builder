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

#pragma once

#include <cstring>
#include <memory>

#include "detail.h"

namespace raster::algorithm {

namespace detail {
    template <typename InputView, typename T>
    requires(!std::is_const_v<T>) && std::same_as<typename InputView::value_type, T>
    Expected<void> copy_views(const InputView& source, const View<T>& destination)
    {
        if (auto valid = validate_pointwise_views(source, destination); !valid) {
            return valid;
        }
        if (destination.width() == 0 || destination.height() == 0 || identical_view_mapping(source, destination)) {
            return {};
        }
        for (unsigned y = 0; y < destination.height(); ++y) {
            for (unsigned x = 0; x < destination.width(); ++x) {
                if constexpr (std::is_trivially_copyable_v<T>) {
                    std::memcpy(std::addressof(destination.pixel({ x, y })), std::addressof(source.pixel({ x, y })), sizeof(T));
                } else {
                    destination.pixel({ x, y }) = source.pixel({ x, y });
                }
            }
        }
        return {};
    }
} // namespace detail

/// Copies equal-sized regions of identical pixel types; rejects partial overlap.
template <detail::ViewSource Source, detail::WritableViewDestination Destination>
requires std::same_as<detail::SourcePixel<Source>, detail::SourcePixel<Destination>>
[[nodiscard]] Expected<void> copy(Source&& source, Destination&& destination)
{
    return detail::copy_views(raster::make_view(source), raster::make_view(destination));
}

template <detail::ViewSource Source>
requires detail::RasterPixel<detail::SourcePixel<Source>>
[[nodiscard]] Expected<radix::Raster<detail::SourcePixel<Source>>> copy(Source&& source)
{
    const auto input = raster::make_view(source);
    return detail::produce_raster<detail::SourcePixel<Source>>(input.size(), [&](const auto& destination) { return detail::copy_views(input, destination); });
}

} // namespace raster::algorithm

#pragma once

#include <cstdint>

#include <radix/raster.h>

#include "Error.h"

namespace raster_store {

inline constexpr unsigned default_tile_side = 4096;

inline Expected<void> validate_dimensions(const glm::uvec2 dimensions)
{
    if (dimensions.x == 0 || dimensions.x != dimensions.y) {
        return Error::fail(Error::Code::InvalidInput, "raster tile dimensions must be positive and square");
    }
    return {};
}

template <typename PixelType>
struct Tile {
    explicit Tile(const unsigned side = default_tile_side)
        : data(side)
        , source_attribution(glm::uvec2(side), std::uint16_t { 0 })
    {
    }

    radix::Raster<PixelType> data;
    radix::Raster<std::uint16_t> source_attribution;
};

} // namespace raster_store

#pragma once

#include "partition.h"
#include "priorities.h"
#include "raster_store/storage.h"
#include "selection.h"
#include "statistics.h"
#include <variant>

namespace rf_merger::produce {

using Key = radix::tile::Id;

// Largest supported zoom gap between a supplier and an output leaf, as in the
// halo reader's ancestor fallback. Inputs are validated against it.
inline constexpr unsigned max_zoom_levels = 30;

template <typename PixelType>
struct Input {
    const raster_store::storage::IndexedStorage<PixelType>* storage;
    const raster_store::io::manifest::Metadata* metadata;
};

template <typename PixelType>
struct Inputs {
    Input<PixelType> left;
    Input<PixelType> right;
    const priorities::Ranks* ranks;
};

// Hard-link the complete input tile at the leaf key.
struct Link {
    selection::Side side;
};

template <typename PixelType>
struct Created {
    raster_store::Tile<PixelType> tile;
    statistics::Category category;
};

template <typename PixelType>
using Produced = std::variant<Link, Created<PixelType>>;

// Reads original suppliers covering the leaf at the same or a coarser zoom,
// aligns them to the leaf grid and applies the selection policy. Only reads
// input storages, so workers may call it concurrently. Throws Error::Exception
// only for failed reads; the value mapping and zoom gaps must already be validated.
template <typename PixelType>
Produced<PixelType> produce(const Inputs<PixelType>& inputs, const partition::Leaf& leaf);

} // namespace rf_merger::produce

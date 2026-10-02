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

#include "produce.h"

#include "raster/ClampedView.h"
#include "raster/algorithm/copy.h"
#include "raster/algorithm/fold.h"
#include "raster/algorithm/zip_transform.h"
#include "raster_store/scaler.h"
#include <libassert/assert.hpp>

namespace rf_merger::produce {
namespace {
    namespace algorithm = raster::algorithm;
    using selection::Side;
    constexpr auto good_filter = algorithm::Resampling::Lanczos3;
    constexpr auto fast_filter = algorithm::Resampling::NearestNeighbourAndBox;

    template <typename T>
    T asserting_unwrap(Expected<T>&& result, std::source_location location = std::source_location::current())
    {
        return Error::asserting_unwrap(std::move(result), location);
    }

    // The leaf's position in the upscaled supplier, and the supplier pixels
    // covering it, in supplier coordinates without halo.
    struct Window {
        unsigned levels;
        glm::u64vec2 position;
        glm::uvec2 origin;
        glm::uvec2 interior;
        unsigned support;
    };

    Window window(const Key& supplier, const Key& leaf, unsigned side)
    {
        const unsigned levels = leaf.zoom_level - supplier.zoom_level;
        ASSERT(levels <= max_zoom_levels, to_string(supplier), to_string(leaf));
        const std::uint64_t factor = std::uint64_t(1) << levels;
        const auto position = glm::u64vec2(leaf.coords & glm::uvec2(factor - 1)) * std::uint64_t(side);
        const auto phase = position & (factor - 1);
        return Window { levels,
            position,
            glm::uvec2(position >> std::uint64_t(levels)),
            glm::uvec2((phase + std::uint64_t(side - 1)) / factor + std::uint64_t(1)),
            asserting_unwrap(algorithm::required_halo(int(levels), good_filter)) };
    }

    template <typename PixelType>
    struct Source {
        Key key;
        // The supplier tile, or only the leaf's window if that leaves the supplier.
        raster_store::Tile<PixelType> tile;
        // Position of the tile's first pixel in supplier pixels.
        glm::ivec2 origin;
        bool attributed;
    };

    template <typename PixelType>
    raster_store::Tile<PixelType> load(const Input<PixelType>& input, const Key& key, std::string_view role)
    {
        auto tile = Error::throwing_unwrap(input.storage->load(key), std::string("read RF merger ") + std::string(role) + " " + to_string(key));
        // The codec guarantees the dimensions.
        ASSERT(tile.data.size() == glm::uvec2(input.metadata->nominal_tile_size) && tile.attribution.size() == tile.data.size(), to_string(key));
        return tile;
    }

    // Fills the part of a window beyond the supplier that lies in the same-zoom
    // neighbour, given in neighbour pixels. A physical tile covering it at the
    // same or a coarser zoom is copied or sampled by nearest neighbour, and
    // physical children of a virtual neighbour are box-reduced. Everything else
    // keeps the replicated supplier edge.
    template <typename PixelType>
    void fill(const Input<PixelType>& input,
        const Key& neighbour,
        glm::uvec2 origin,
        glm::uvec2 size,
        raster::View<PixelType> data,
        raster::View<std::uint16_t> attribution)
    {
        const auto& index = input.storage->index();
        const unsigned side = input.metadata->nominal_tile_size;
        const auto value_mapping = input.metadata->value_mapping;
        if (asserting_unwrap(index.get(neighbour)) == store::NodeStatus::Virtual) {
            const unsigned extent = side / 2;
            const auto children = raster_store::StoreTraits::children(neighbour);
            ASSERT(children.has_value(), to_string(neighbour));
            for (const auto& child : *children) {
                const auto child_origin = (child.coords & glm::uvec2(1)) * extent;
                const auto first = glm::max(origin, child_origin);
                const auto last = glm::min(origin + size, child_origin + extent);
                if (glm::any(glm::greaterThanEqual(first, last)) || asserting_unwrap(index.get(child)) != store::NodeStatus::Leaf)
                    continue;
                const auto source = load(input, child, "halo child");
                const glm::i64vec2 offset(first - child_origin);
                const auto output = asserting_unwrap(raster::make_view(data, first - origin, last - first));
                const auto output_attribution = asserting_unwrap(raster::make_view(attribution, first - origin, last - first));
                asserting_unwrap(raster_store::scale(source.data, source.attribution, 0, -1, fast_filter, offset, value_mapping, output, output_attribution));
            }
            return;
        }
        // The physical tile covering the neighbour at the same or a coarser zoom.
        const auto covering = partition::supplier(index, neighbour);
        if (!covering)
            return;
        const auto source = load(input, *covering, "halo source");
        const unsigned gap = neighbour.zoom_level - covering->zoom_level;
        // The region's position in the upscaled covering tile. Zero levels
        // copy the region exactly, so a same-zoom neighbour is copied.
        const auto offset
            = (glm::i64vec2(neighbour.coords) - (glm::i64vec2(covering->coords) << std::int64_t(gap))) * std::int64_t(side) + glm::i64vec2(origin);
        asserting_unwrap(raster_store::scale(source.data, source.attribution, 0, int(gap), fast_filter, offset, value_mapping, data, attribution));
    }

    // Assembles the supplier window [origin, origin + size) at the supplier's
    // resolution. Halo pixels come from the supplier's neighbours, wrapping at
    // the antimeridian; beyond the poles the supplier edge is replicated.
    template <typename PixelType>
    raster_store::Tile<PixelType> assemble(
        const Input<PixelType>& input, const Key& supplier, const raster_store::Tile<PixelType>& tile, glm::ivec2 origin, glm::uvec2 size)
    {
        const int side = int(input.metadata->nominal_tile_size);
        // Neighbours beyond the eight adjacent tiles are never needed.
        ASSERT(glm::all(glm::greaterThanEqual(origin, glm::ivec2(-side))) && glm::all(glm::lessThanEqual(origin + glm::ivec2(size), glm::ivec2(2 * side))));
        raster_store::Tile<PixelType> result(0);
        result.data = asserting_unwrap(algorithm::copy(asserting_unwrap(raster::make_clamped_view(tile.data, origin, size))));
        result.attribution = asserting_unwrap(algorithm::copy(asserting_unwrap(raster::make_clamped_view(tile.attribution, origin, size))));
        const auto tiles = std::int64_t(1) << supplier.zoom_level;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const glm::ivec2 offset = glm::ivec2(dx, dy) * side;
                const auto first = glm::max(origin, offset);
                const auto last = glm::min(origin + glm::ivec2(size), offset + side);
                const auto y = std::int64_t(supplier.coords.y) + dy;
                if ((dx == 0 && dy == 0) || glm::any(glm::greaterThanEqual(first, last)) || y < 0 || y >= tiles)
                    continue;
                const Key neighbour { supplier.zoom_level, { unsigned((std::int64_t(supplier.coords.x) + dx + tiles) % tiles), unsigned(y) } };
                const auto region = glm::uvec2(last - first);
                const auto destination = glm::uvec2(first - origin);
                const auto data = asserting_unwrap(raster::make_view(result.data, destination, region));
                const auto attribution = asserting_unwrap(raster::make_view(result.attribution, destination, region));
                fill(input, neighbour, glm::uvec2(first - offset), region, data, attribution);
            }
        }
        return result;
    }

    // Reads the supplier and, if the leaf's scaling window leaves it, replaces
    // it by the assembled window.
    template <typename PixelType>
    Source<PixelType> read(const Input<PixelType>& input, const Key& supplier, const Key& leaf)
    {
        const unsigned side = input.metadata->nominal_tile_size;
        auto tile = load(input, supplier, "supplier");
        const bool attributed = algorithm::fold(tile.attribution, false, [](bool any, std::uint16_t attribution) { return any || attribution != 0; });
        Source<PixelType> result { supplier, std::move(tile), glm::ivec2(0), attributed };
        if (supplier == leaf) {
            return result;
        }
        const auto geometry = window(supplier, leaf, side);
        const auto origin = glm::ivec2(geometry.origin) - int(geometry.support);
        const auto size = geometry.interior + glm::uvec2(2 * geometry.support);
        if (glm::any(glm::lessThan(origin, glm::ivec2(0))) || glm::any(glm::greaterThan(glm::uvec2(origin) + size, glm::uvec2(side)))) {
            result.tile = assemble(input, supplier, result.tile, origin, size);
            result.origin = origin;
        }
        return result;
    }

    // Aligns a supplier to the leaf grid: native tiles are used as they are,
    // coarser ones are upscaled directly with the windowed paired scaler.
    // The window and value mapping were validated before production.
    template <typename PixelType>
    raster_store::Tile<PixelType> align(Source<PixelType> source, const Key& leaf, const raster_store::io::manifest::Metadata& metadata)
    {
        if (source.key == leaf) {
            return std::move(source.tile);
        }
        const unsigned side = metadata.nominal_tile_size;
        const auto geometry = window(source.key, leaf, side);
        const auto offset = glm::i64vec2(geometry.position) - glm::i64vec2(source.origin) * (std::int64_t(1) << geometry.levels);
        const auto& [data, attribution] = source.tile;
        const auto value_mapping = metadata.value_mapping;
        raster_store::Tile<PixelType> output(side);
        asserting_unwrap(raster_store::scale(data, attribution, 0, int(geometry.levels), good_filter, offset, value_mapping, output.data, output.attribution));
        return output;
    }

    statistics::Category only(Side side) { return side == Side::Left ? statistics::Category::LeftOnly : statistics::Category::RightOnly; }

    template <typename PixelType>
    Produced<PixelType> whole(Source<PixelType> source, Side side, const Key& leaf, const raster_store::io::manifest::Metadata& metadata)
    {
        if (source.key == leaf) {
            return Link { side };
        }
        return Created<PixelType> { align(std::move(source), leaf, metadata), only(side) };
    }
} // namespace

template <typename PixelType>
Produced<PixelType> produce(const Inputs<PixelType>& inputs, const partition::Leaf& leaf)
{
    ASSERT(leaf.left || leaf.right, to_string(leaf.key));
    const auto& metadata = *inputs.left.metadata;
    if (!leaf.left || !leaf.right) {
        const Side side = leaf.left ? Side::Left : Side::Right;
        const auto& input = leaf.left ? inputs.left : inputs.right;
        const auto supplier = leaf.left ? *leaf.left : *leaf.right;
        if (supplier == leaf.key) {
            return Link { side };
        }
        return whole(read(input, supplier, leaf.key), side, leaf.key, metadata);
    }
    auto left = read(inputs.left, *leaf.left, leaf.key);
    auto right = read(inputs.right, *leaf.right, leaf.key);
    const auto winner = selection::whole_tile({ left.attributed, left.key.zoom_level }, { right.attributed, right.key.zoom_level });
    if (winner) {
        return *winner == Side::Left ? whole(std::move(left), Side::Left, leaf.key, metadata) : whole(std::move(right), Side::Right, leaf.key, metadata);
    }
    const Key left_key = left.key;
    const Key right_key = right.key;
    const selection::Pixel pixel(*inputs.ranks, left_key.zoom_level, right_key.zoom_level);
    auto aligned_left = align(std::move(left), leaf.key, metadata);
    auto aligned_right = align(std::move(right), leaf.key, metadata);
    const auto choice = asserting_unwrap(algorithm::zip_transform(
        aligned_left.attribution, aligned_right.attribution, [&pixel](std::uint16_t left_attribution, std::uint16_t right_attribution) {
            return std::uint8_t(pixel.right_wins(left_attribution, right_attribution));
        }));
    const auto right_pixels = algorithm::fold(choice, std::uint64_t(0), [](std::uint64_t count, std::uint8_t right_wins) { return count + right_wins; });
    const auto pixels = std::uint64_t(choice.width()) * choice.height();
    if (right_pixels == 0) {
        if (left_key == leaf.key) {
            return Link { Side::Left };
        }
        return Created<PixelType> { std::move(aligned_left), statistics::Category::LeftOnly };
    }
    if (right_pixels == pixels) {
        if (right_key == leaf.key) {
            return Link { Side::Right };
        }
        return Created<PixelType> { std::move(aligned_right), statistics::Category::RightOnly };
    }
    raster_store::Tile<PixelType> output(metadata.nominal_tile_size);
    // Selects data and attribution alike.
    const auto select = [](std::uint8_t right_wins, const auto& left_value, const auto& right_value) { return right_wins ? right_value : left_value; };
    asserting_unwrap(algorithm::zip_transform(choice, aligned_left.data, aligned_right.data, select, output.data));
    asserting_unwrap(algorithm::zip_transform(choice, aligned_left.attribution, aligned_right.attribution, select, output.attribution));
    return Created<PixelType> { std::move(output), statistics::Category::Mixed };
}

template Produced<float> produce(const Inputs<float>&, const partition::Leaf&);
template Produced<glm::u8vec3> produce(const Inputs<glm::u8vec3>&, const partition::Leaf&);

} // namespace rf_merger::produce

#pragma once

#include <bit>
#include <limits>
#include <optional>
#include <utility>

#include <libassert/assert.hpp>

#include "raster/ClampedView.h"
#include <map>
#include <vector>

#include "raster_store/scaler.h"
#include "raster_store/storage.h"

namespace raster_store {
namespace halo_detail {
    struct Rectangle {
        glm::uvec2 origin;
        glm::uvec2 size;
    };
    struct Assignment {
        radix::tile::Id neighbour;
        Rectangle region;
        glm::uvec2 destination;
        glm::uvec2 source_origin;
    };
    inline bool physical(const store::Index<StoreTraits>& index, const radix::tile::Id& id)
    {
        const auto status = index.get(id);
        return status && *status && **status != store::NodeStatus::Virtual;
    }
    inline std::optional<Rectangle> intersect(Rectangle left, Rectangle right)
    {
        const auto origin = glm::max(left.origin, right.origin);
        const auto end = glm::min(left.origin + left.size, right.origin + right.size);
        if (origin.x >= end.x || origin.y >= end.y)
            return std::nullopt;
        return Rectangle { origin, end - origin };
    }
} // namespace halo_detail

/// Read a physical tile and its halo. Metadata must belong to the supplied storage.
template <typename T>
Expected<Tile<T>> read_tile_with_halo(const storage::IndexedStorage<T>& storage,
    const io::manifest::Metadata& metadata,
    const radix::tile::Id& id,
    unsigned halo_width,
    raster::algorithm::Interpolation interpolation)
{
    namespace algorithm = raster::algorithm;
    using namespace halo_detail;
    if (auto valid = io::manifest::validate(metadata); !valid)
        return Error::propagate(std::move(valid));
    if (!StoreTraits::is_valid(id))
        return Error::fail(Error::Code::InvalidInput, "invalid halo centre tile ID");
    const unsigned side = metadata.nominal_tile_size;
    DEBUG_ASSERT(side >= 64);
    if (halo_width > side || halo_width > ((std::numeric_limits<unsigned>::max)() - side) / 2 || (metadata.halo_width != 0 && halo_width > metadata.halo_width))
        return Error::fail(Error::Code::InvalidInput, "requested halo exceeds available width");
    auto centre = storage.load(id);
    if (!centre)
        return Error::propagate(std::move(centre), "read halo centre");
    const auto validate_tile = [&](const Tile<T>& tile) -> Expected<void> {
        if (tile.data.size() != glm::uvec2(metadata.stored_tile_size) || tile.source_attribution.size() != tile.data.size())
            return Error::fail(Error::Code::CorruptData, "tile dimensions disagree with halo metadata");
        return {};
    };
    if (auto valid = validate_tile(*centre); !valid)
        return Error::propagate(std::move(valid));
    const unsigned output_side = side + 2 * halo_width;
    auto pair = scaler::detail::produce_pair<T>(glm::uvec2(output_side), [](auto&, auto&) -> Expected<void> { return {}; });
    if (!pair)
        return Error::propagate(std::move(pair));
    Tile<T> output(0);
    output.data = std::move(pair->first);
    output.source_attribution = std::move(pair->second);
    output.source_attribution.fill(0);
    if (metadata.halo_width != 0 || halo_width == 0) {
        const auto origin = glm::uvec2(metadata.halo_width - halo_width);
        if (auto result = algorithm::copy(*raster::make_view(centre->data, origin, output.data.size()), output.data); !result)
            return Error::propagate(std::move(result));
        if (auto result = algorithm::copy(*raster::make_view(centre->source_attribution, origin, output.data.size()), output.source_attribution); !result)
            return Error::propagate(std::move(result));
        return output;
    }
    if (auto result = algorithm::copy(centre->data, *raster::make_view(output.data, glm::uvec2(halo_width), glm::uvec2(side))); !result)
        return Error::propagate(std::move(result));
    if (auto result = algorithm::copy(centre->source_attribution, *raster::make_view(output.source_attribution, glm::uvec2(halo_width), glm::uvec2(side)));
        !result)
        return Error::propagate(std::move(result));

    std::map<radix::tile::Id, std::vector<Assignment>> assignments;
    const auto replicate = [&](Rectangle region, glm::uvec2 destination) -> Expected<void> {
        auto source = raster::make_clamped_view(centre->data, glm::ivec2(destination) - glm::ivec2(halo_width), region.size);
        if (!source)
            return Error::propagate(std::move(source));
        return algorithm::copy(*source, *raster::make_view(output.data, destination, region.size));
    };
    // Every neighbour covers at most one side/corner rectangle. Resolve index
    // coverage first, then group disjoint assignments to guarantee one source read.
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0)
                continue;
            const glm::uvec2 size(dx == 0 ? side : halo_width, dy == 0 ? side : halo_width);
            const glm::uvec2 destination(dx < 0 ? 0 : dx == 0 ? halo_width : halo_width + side, dy < 0 ? 0 : dy == 0 ? halo_width : halo_width + side);
            const Rectangle region { { dx < 0 ? side - halo_width : 0, dy < 0 ? side - halo_width : 0 }, size };
            const unsigned maximum = id.zoom_level == 32 ? (std::numeric_limits<unsigned>::max)() : (1u << id.zoom_level) - 1;
            if ((dy < 0 && id.coords.y == 0) || (dy > 0 && id.coords.y == maximum)) {
                if (auto result = replicate(region, destination); !result)
                    return Error::propagate(std::move(result));
                continue;
            }
            const radix::tile::Id neighbour { id.zoom_level, { (id.coords.x + static_cast<unsigned>(dx)) & maximum, id.coords.y + static_cast<unsigned>(dy) } };
            std::optional<radix::tile::Id> ancestor = std::nullopt;
            for (auto candidate = StoreTraits::parent(neighbour); candidate; candidate = StoreTraits::parent(*candidate)) {
                if (physical(storage.index(), *candidate)) {
                    ancestor = candidate;
                    break;
                }
            }
            const auto visit = [&](const auto& self, const radix::tile::Id& node, glm::uvec2 origin, unsigned extent, unsigned depth) -> Expected<void> {
                const auto overlap = intersect(region, { origin, glm::uvec2(extent) });
                if (!overlap)
                    return {};
                const auto target = destination + overlap->origin - region.origin;
                if (physical(storage.index(), node)) {
                    assignments[node].push_back({ neighbour, *overlap, target, origin });
                    return {};
                }
                const auto status = storage.index().get(node);
                const auto children = StoreTraits::children(node);
                if (depth < 4 && status && *status && children) {
                    for (const auto& child : *children) {
                        const auto child_origin = origin + (child.coords & glm::uvec2(1)) * (extent / 2);
                        if (auto result = self(self, child, child_origin, extent / 2, depth + 1); !result)
                            return result;
                    }
                    return {};
                }
                if (ancestor)
                    assignments[*ancestor].push_back({ neighbour, *overlap, target, {} });
                else
                    return replicate(*overlap, target);
                return {};
            };
            if (auto result = visit(visit, neighbour, {}, side, 0); !result)
                return Error::propagate(std::move(result));
        }
    }
    for (const auto& [source_id, regions] : assignments) {
        std::optional<Tile<T>> loaded;
        if (source_id != id) {
            auto tile = storage.load(source_id);
            if (!tile)
                return Error::propagate(std::move(tile), "read halo source");
            if (auto valid = validate_tile(*tile); !valid)
                return Error::propagate(std::move(valid));
            loaded.emplace(std::move(*tile));
        }
        const auto& source = loaded ? *loaded : *centre;
        for (const auto& assignment : regions) {
            const auto data = *raster::make_view(output.data, assignment.destination, assignment.region.size);
            const auto attribution = *raster::make_view(output.source_attribution, assignment.destination, assignment.region.size);
            if (source_id.zoom_level == id.zoom_level) {
                if (auto result = algorithm::copy(*raster::make_view(source.data, assignment.region.origin, data.size()), data); !result)
                    return Error::propagate(std::move(result));
                if (auto result = algorithm::copy(*raster::make_view(source.source_attribution, assignment.region.origin, data.size()), attribution); !result)
                    return Error::propagate(std::move(result));
            } else if (source_id.zoom_level > id.zoom_level) {
                auto result = scaler::scale(source.data,
                    source.source_attribution,
                    0,
                    -int(source_id.zoom_level - id.zoom_level),
                    interpolation,
                    algorithm::Filter::Box,
                    assignment.region.origin - assignment.source_origin,
                    metadata.value_mapping,
                    data,
                    attribution);
                if (!result)
                    return Error::propagate(std::move(result));
            } else {
                const unsigned gap = id.zoom_level - source_id.zoom_level;
                auto factor = algorithm::detail::scale_factor(gap);
                if (!factor)
                    return Error::propagate(std::move(factor));
                const auto tile_offset = assignment.neighbour.coords & glm::uvec2(*factor - 1);
                const unsigned side_bits = std::countr_zero(side);
                glm::uvec2 source_origin;
                glm::uvec2 phase;
                if (gap <= side_bits) {
                    source_origin = tile_offset * (side >> gap) + assignment.region.origin / *factor;
                    phase = assignment.region.origin % *factor;
                } else {
                    source_origin = tile_offset >> (gap - side_bits);
                    phase = (tile_offset & glm::uvec2((1u << (gap - side_bits)) - 1)) * side + assignment.region.origin;
                }
                const auto last = phase + assignment.region.size - glm::uvec2(1);
                const auto interior = last / *factor + glm::uvec2(1);
                const unsigned support = interpolation == algorithm::Interpolation::Bilinear ? 1 : 0;
                // Clamping bounds remain those of the complete supplying tile.
                auto input = raster::make_clamped_view(source.data, glm::ivec2(source_origin) - glm::ivec2(support), interior + glm::uvec2(2 * support));
                auto input_attribution
                    = raster::make_clamped_view(source.source_attribution, glm::ivec2(source_origin) - glm::ivec2(support), interior + glm::uvec2(2 * support));
                if (!input)
                    return Error::propagate(std::move(input));
                if (!input_attribution)
                    return Error::propagate(std::move(input_attribution));
                auto result = scaler::scale(
                    *input, *input_attribution, support, int(gap), interpolation, algorithm::Filter::Box, phase, metadata.value_mapping, data, attribution);
                if (!result)
                    return Error::propagate(std::move(result));
            }
        }
    }
    return output;
}
} // namespace raster_store

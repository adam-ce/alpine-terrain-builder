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

#include "planning.h"

#include <algorithm>
#include <libassert/assert.hpp>
#include <optional>
#include "raster_store/StoreTraits.h"

namespace rf_builder::gdal::planning {
namespace {
std::optional<Bounds> intersection(const Bounds& a, const Bounds& b)
{
    Bounds result { glm::max(a.min, b.min), glm::min(a.max, b.max) };
    if (result.min.x >= result.max.x || result.min.y >= result.max.y) {
        return std::nullopt;
    }
    return result;
}
}

Cursor::Cursor(unsigned side, const std::vector<Bounds>& source_bounds, const std::vector<Bounds>& mask_bounds, PixelSize pixel_size, unsigned halo_width)
    : m_side(side)
    , m_halo_width(halo_width)
    , m_pixel_size(std::move(pixel_size))
    , m_pending { raster_store::StoreTraits::root() }
{
    for (const auto& source : source_bounds) {
        for (const auto& mask : mask_bounds) {
            if (auto overlap = intersection(source, mask)) {
                m_coverage.push_back(*overlap);
            }
        }
    }
}
std::optional<radix::tile::Id> Cursor::next(const std::function<void()>& poll)
{
    ASSERT(m_side > 0);
    while (!m_pending.empty()) {
        if (poll) {
            poll();
        }
        const auto key = m_pending.back();
        m_pending.pop_back();
        const auto tile_bounds = RasterTransform::tile_bounds(key);
        bool intersects = false;
        double ratio = 0;
        const double spacing = tile_bounds.width() / m_side;
        const double half = RasterTransform::world_half_extent;
        const double extent = m_halo_width * spacing;
        const Bounds expanded { tile_bounds.min - glm::dvec2(extent), tile_bounds.max + glm::dvec2(extent) };
        // Compare canonical coverage against shifted copies of the expanded
        // candidate. Halo widths >= one world simply cover every longitude.
        for (const auto& region : m_coverage) {
            for (int branch : { -1, 0, 1 }) {
                Bounds candidate = expanded;
                if (expanded.width() >= 2 * half) {
                    if (branch != 0) {
                        continue;
                    }
                    candidate.min.x = -half;
                    candidate.max.x = half;
                } else {
                    candidate.min.x += branch * 2 * half;
                    candidate.max.x += branch * 2 * half;
                }
                candidate.min.y = (std::max)(candidate.min.y, -half);
                candidate.max.y = (std::min)(candidate.max.y, half);
                const auto overlap = intersection(candidate, region);
                if (!overlap) {
                    continue;
                }
                intersects = true;
                const auto pixel = Error::throwing_unwrap(m_pixel_size(*overlap), "estimate resolution for RF tile " + to_string(key));
                const double smallest = (std::min)(pixel.x, pixel.y);
                if (!(smallest > 0)) {
                    Error::raise(Error::Code::InvalidInput, "nonpositive RF source pixel size for tile " + to_string(key));
                }
                ratio = (std::max)(ratio, spacing / smallest);
                if (ratio > sampling_limit) {
                    break;
                }
            }
            if (ratio > sampling_limit) { break; }
        }
        if (!intersects) { continue; }
        if (ratio <= sampling_limit) {
            return std::optional(key);
        } else {
            const auto children = raster_store::StoreTraits::children(key);
            if (!children) {
                Error::raise(Error::Code::Unsupported, "RF sampling limit cannot be met at maximum zoom " + to_string(key));
            }
            // Reverse insertion gives stable northwest-first traversal.
            m_pending.insert(m_pending.end(), children->rbegin(), children->rend());
        }
    }
    return std::nullopt;
}

void traverse(const unsigned side,
    const std::vector<Bounds>& source_bounds,
    const std::vector<Bounds>& mask_bounds,
    const PixelSize& pixel_size,
    const Visit& visit,
    const std::function<void()>& checkpoint,
    unsigned halo_width)
{
    Cursor cursor(side, source_bounds, mask_bounds, pixel_size, halo_width);
    while (const auto key = cursor.next(checkpoint)) {
        visit(*key);
    }
}

} // namespace rf_builder::gdal::planning

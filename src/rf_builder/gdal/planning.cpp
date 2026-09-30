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
#include <cmath>
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

double directional_stretch(const glm::dvec2 column, const glm::dvec2 row)
{
    const double a = glm::dot(column, column);
    const double b = glm::dot(column, row);
    const double d = glm::dot(row, row);
    return std::sqrt((a + d + std::hypot(a - d, 2 * b)) / 2);
}

Expected<double> estimate(const Bounds& region, const double pixel_spacing, const Transform& transform)
{
    double maximum = 0;
    double minimum = INFINITY;
    // Quarter-output-pixel finite differences, with inward differences at the
    // world edges. Refine 3x3 -> 9x9 -> 17x17 near the limit or high variation.
    const double step = pixel_spacing / 4;
    for (unsigned count : { 3u, 9u, 17u }) {
        const double previous = maximum;
        for (unsigned y = 0; y < count; ++y) {
            for (unsigned x = 0; x < count; ++x) {
                const glm::dvec2 point = glm::mix(region.min, region.max,
                    glm::dvec2(double(x) / (count - 1), double(y) / (count - 1)));
                const double dx = point.x + step <= RasterTransform::world_half_extent ? step : -step;
                const double dy = point.y + step <= RasterTransform::world_half_extent ? step : -step;
                auto centre = transform(point);
                auto horizontal = transform(point + glm::dvec2(dx, 0));
                auto vertical = transform(point + glm::dvec2(0, dy));
                if (!centre) { return Error::propagate(std::move(centre)); }
                if (!horizontal) { return Error::propagate(std::move(horizontal)); }
                if (!vertical) { return Error::propagate(std::move(vertical)); }
                const double ratio = directional_stretch((*horizontal - *centre) * (pixel_spacing / dx),
                    (*vertical - *centre) * (pixel_spacing / dy));
                if (!std::isfinite(ratio)) {
                    return Error::fail(Error::Code::InvalidInput, "nonfinite RF sampling ratio");
                }
                maximum = (std::max)(maximum, ratio);
                minimum = (std::min)(minimum, ratio);
            }
        }
        if (maximum > sampling_limit || (maximum < 0.9 * sampling_limit && maximum - minimum <= 0.05 * maximum)
            || (count > 3 && maximum - previous <= 0.001 * maximum && maximum < 0.99 * sampling_limit)) {
            break;
        }
    }
    return maximum;
}

Cursor::Cursor(unsigned side, const std::vector<Bounds>& source_bounds, const std::vector<Bounds>& mask_bounds, Transform transform, unsigned halo_width)
    : m_side(side)
    , m_halo_width(halo_width)
    , m_transform(std::move(transform))
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
                ratio
                    = (std::max)(ratio, Error::throwing_unwrap(estimate(*overlap, spacing, m_transform), "estimate resolution for RF tile " + to_string(key)));
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
    const Transform& transform,
    const Visit& visit,
    const std::function<void()>& checkpoint,
    unsigned halo_width)
{
    Cursor cursor(side, source_bounds, mask_bounds, transform, halo_width);
    while (const auto key = cursor.next(checkpoint)) {
        visit(*key);
    }
}

} // namespace rf_builder::gdal::planning

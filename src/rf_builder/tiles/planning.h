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
#include "RasterTransform.h"
#include "run.h"
namespace rf_builder::tiles::planning {
using Bounds = RasterTransform::Bounds;
// Disjoint normalized Mercator rectangles, built once from conservative mask
// bounds. Their union supports both spatial pruning and additive area weights.
class Coverage {
public:
    explicit Coverage(const std::vector<Bounds>& mercator_bounds);
    double weight(const run::Key& key) const;
    bool intersects(const run::Key& key, const run::Key& region) const;
    run::Subdivide children(const run::Key& key) const;
    static Bounds bounds(const run::Key& key);

private:
    std::vector<Bounds> m_rectangles;
};
class Cursor {
public:
    Cursor(const Coverage& coverage, unsigned zoom, run::Key region = { 0, { 0, 0 } });
    Expected<std::optional<run::Key>> next(const run::Poll& poll = {});

private:
    const Coverage& m_coverage;
    unsigned m_zoom;
    run::Key m_region;
    std::vector<run::Key> m_pending { { 0, { 0, 0 } } };
};
} // namespace rf_builder::tiles::planning

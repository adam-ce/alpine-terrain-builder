/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
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

#include <vector>

#include <radix/geometry.h>

namespace polygon {

using Triangle2d = radix::geometry::Triangle<2, double>;

// Appends the overlap of two triangles, fanned into triangles. Degenerate pieces are dropped, so
// nothing is appended when they are disjoint, meet in at most a segment, or either is degenerate.
void clip_triangle(const Triangle2d &subject, const Triangle2d &clip, std::vector<Triangle2d> &out);

} // namespace polygon

#pragma once

#include <vector>

#include <radix/geometry.h>

namespace polygon {

using Triangle2d = radix::geometry::Triangle<2, double>;

// Appends the overlap of two triangles, fanned into triangles. Degenerate pieces are dropped, so
// nothing is appended when they are disjoint, meet in at most a segment, or either is degenerate.
void clip_triangle(const Triangle2d &subject, const Triangle2d &clip, std::vector<Triangle2d> &out);

} // namespace polygon

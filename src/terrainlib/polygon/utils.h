#pragma once

#include "polygon/Polygon.h"

namespace polygon {
bool is_planar(const Polygon3d &poly, double eps = 1e-9);
}

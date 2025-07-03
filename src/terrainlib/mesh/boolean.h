#pragma once

#include "mesh/SimpleMesh.h"

namespace mesh {

struct IntersectionAndDifference {
    SimpleMesh intersection;
    SimpleMesh difference; // a - b
};
IntersectionAndDifference intersection_and_difference(const SimpleMesh &a, const SimpleMesh &b);

}

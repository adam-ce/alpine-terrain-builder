
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <libassert/assert.hpp>

#include "mesh/boolean.h"
#include "mesh/convert.h"
#include "mesh/cgal.h"

namespace mesh {

IntersectionAndDifference intersection_and_difference(const SimpleMesh &a, const SimpleMesh &b) {
    ASSERT(!a.has_uvs());
    ASSERT(!b.has_uvs());

    cgal::Mesh cgal_a = convert::to_cgal_mesh(a);
    cgal::Mesh cgal_b = convert::to_cgal_mesh(b);

    cgal::Mesh cgal_intersection;
    cgal::Mesh cgal_difference;
    std::array<std::optional<cgal::Mesh*>, 4> cgal_out;
    const size_t intersection_index = CGAL::Polygon_mesh_processing::Corefinement::Boolean_operation_type::INTERSECTION;
    const size_t difference_index = CGAL::Polygon_mesh_processing::Corefinement::Boolean_operation_type::TM1_MINUS_TM2;
    cgal_out[intersection_index] = &cgal_intersection;
    cgal_out[difference_index] = &cgal_difference;

    const std::array<bool, 4> cgal_result = 
        CGAL::Polygon_mesh_processing::corefine_and_compute_boolean_operations(cgal_a, cgal_b, cgal_out);
    if (!cgal_result[intersection_index] || !cgal_result[difference_index]) {
        throw std::runtime_error("corefine_and_compute_boolean_operations failed");
    }

    IntersectionAndDifference result;
    result.intersection = convert::to_simple_mesh(cgal_intersection);
    result.difference = convert::to_simple_mesh(cgal_difference);   
    return result;
}

}
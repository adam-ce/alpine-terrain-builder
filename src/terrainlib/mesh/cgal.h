/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Martin Braunsperger
 * Copyright (C) 2024 Adam Celarek-Litofcenko
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

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Simple_cartesian.h>
#include <CGAL/Surface_mesh/Surface_mesh.h>

namespace cgal {
#define DEFINE_KERNEL(K)                                                       \
    using Kernel = K;                                                          \
    using Point2 = Kernel::Point_2;                                            \
    using Point3 = Kernel::Point_3;                                            \
    using Mesh = CGAL::Surface_mesh<Point3>;                                   \
    using VertexIndex = Mesh::Vertex_index;                                    \
    using FaceIndex = Mesh::Face_index;                                        \
    using VertexDescriptor = boost::graph_traits<Mesh>::vertex_descriptor;     \
    using HalfedgeDescriptor = boost::graph_traits<Mesh>::halfedge_descriptor; \
    using EdgeDescriptor = boost::graph_traits<Mesh>::edge_descriptor;         \
    using FaceDescriptor = boost::graph_traits<Mesh>::face_descriptor;

namespace kernel {
namespace epick {
    DEFINE_KERNEL(CGAL::Exact_predicates_inexact_constructions_kernel);
}

namespace epeck {
    DEFINE_KERNEL(CGAL::Exact_predicates_exact_constructions_kernel);
}

namespace simple {
    DEFINE_KERNEL(CGAL::Simple_cartesian<double>);
}
}

DEFINE_KERNEL(CGAL::Exact_predicates_inexact_constructions_kernel)

#undef DEFINE_KERNEL

} // namespace cgal

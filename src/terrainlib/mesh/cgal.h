#pragma once

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Simple_cartesian.h>
#include <CGAL/Surface_mesh/Surface_mesh.h>

namespace cgal {
#define DEFINE_KERNEL(K)                                                              \
    using Kernel = K;                                                                 \
    using Point2 = Kernel::Point_2;                                                   \
    using Point3 = Kernel::Point_3;                                                   \
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

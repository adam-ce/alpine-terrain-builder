#ifndef SIMPLIFY_H
#define SIMPLIFY_H

#include "pch.h"

using namespace kernel::epick;
namespace simplify {
// We use a different uv map type here, because we need this one to be attached to the mesh
// as otherwise the entries for removed vertices are not removed during garbage collection.
// We could use the same type as in uv_map but this would require a custom visitor or similar.
typedef SurfaceMesh::Property_map<VertexDescriptor, Point2> AttachedUvPropertyMap;

cv::Mat simplify_texture(const cv::Mat& texture, glm::uvec2 target_resolution);

void simplify_mesh_texture(SimpleMesh& mesh, glm::uvec2 target_resolution);

enum class Algorithm {
    GarlandHeckbert,
    LindstromTurk
};

struct VertexRatio {
    double ratio;
};
struct EdgeRatio {
    double ratio;
};
struct FaceRatio {
    double ratio;
};
struct AbsoluteError {
    double error_bound;
};
struct RelativeError {
    double error_bound;
};

using StopCondition = std::variant<VertexRatio, EdgeRatio, FaceRatio, AbsoluteError, RelativeError>;

struct Options {
    Algorithm algorithm = Algorithm::LindstromTurk;
    bool lock_borders = true;
};

struct Result {
    SimpleMesh mesh;
    double max_absolute_error;
};

Result simplify_mesh(const SimpleMesh &mesh, std::span<const StopCondition> stop_conditions, Options options = Options());
inline Result simplify_mesh(const SimpleMesh &mesh, const StopCondition& stop_condition, Options options = Options()) {
    return simplify_mesh(mesh, std::span<const StopCondition>{&stop_condition, 1}, options);
}
}

// fmt support
template <> struct fmt::formatter<simplify::Algorithm>: formatter<string_view> {
    auto format(const simplify::Algorithm& algorithm, format_context& ctx) const;
};

#endif

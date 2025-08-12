#include "mesh/merge.h"
#include "mesh/merging/mapping.h"
#include "mesh/merging/VertexMapping.h"

namespace mesh {

SimpleMesh merge(const std::span<const std::reference_wrapper<const SimpleMesh>> meshes) {
    switch (meshes.size()) {
    case 0:
        return {};
    case 1:
        return meshes[0];
    default:
        const merging::VertexMapping mapping = merging::create_mapping(meshes);
        return merging::apply_mapping(meshes, mapping);
    }
}

SimpleMesh merge(const std::span<const std::reference_wrapper<const SimpleMesh>> meshes, double distance_epsilon) {
    switch (meshes.size()) {
    case 0:
        return {};
    case 1:
        return meshes[0];
    default:
        const merging::VertexMapping mapping = merging::create_mapping(meshes, distance_epsilon);
        return merging::apply_mapping(meshes, mapping);
    }
}

SimpleMesh merge(const std::span<const std::reference_wrapper<const SimpleMesh>> meshes, merging::VertexDeduplicate<3, double, merging::VertexId> &deduplicate) {
    switch (meshes.size()) {
    case 0:
        return {};
    case 1:
        return meshes[0];
    default:
        const merging::VertexMapping mapping = merging::create_mapping(meshes, deduplicate);
        return merging::apply_mapping(meshes, mapping);
    }
}

} // namespace mesh

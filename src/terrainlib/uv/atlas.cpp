#include <memory>

#include <xatlas/xatlas.h>

#include "atlas.h"
#include "enumerate.h"
#include "geometry_utils.h"
#include "glm_utils.h"
#include "log.h"

namespace uv {

namespace {
using AtlasHandle = std::unique_ptr<xatlas::Atlas, decltype(&xatlas::Destroy)>;

AtlasHandle pack_charts(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec3> positions,
    const AtlasOptions &options) {
    // xatlas works in floats, and judges degeneracy against an absolute epsilon.
    const std::vector<glm::vec3> normalized = to_approximate_normalized(positions);

    xatlas::MeshDecl declaration;
    declaration.vertexCount = normalized.size();
    declaration.vertexPositionData = normalized.data();
    declaration.vertexPositionStride = sizeof(glm::vec3);
    declaration.indexCount = triangles.size() * 3;
    declaration.indexData = triangles.data();
    declaration.indexFormat = xatlas::IndexFormat::UInt32;

    AtlasHandle atlas(xatlas::Create(), xatlas::Destroy);
    const xatlas::AddMeshError error = xatlas::AddMesh(atlas.get(), declaration);
    ASSERT(error == xatlas::AddMeshError::Success, xatlas::StringForEnum(error));

    xatlas::ComputeCharts(atlas.get());

    xatlas::PackOptions pack_options;
    pack_options.padding = options.padding;
    pack_options.resolution = options.resolution;
    xatlas::PackCharts(atlas.get(), pack_options);

    return atlas;
}
} // namespace

Atlas build_atlas(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec3> positions,
    const AtlasOptions options) {
    if (triangles.empty()) {
        return {};
    }

    const AtlasHandle packed = pack_charts(triangles, positions, options);
    if (packed->width == 0 || packed->height == 0) {
        // Nothing was charted, so there is no atlas to address.
        LOG_WARN("Unwrap produced no charts for {} triangles", triangles.size());
        return {};
    }
    DEBUG_ASSERT(packed->meshCount == 1);
    DEBUG_ASSERT(packed->atlasCount == 1);

    const xatlas::Mesh &mesh = packed->meshes[0];
    DEBUG_ASSERT(mesh.indexCount == triangles.size() * 3);

    const glm::uvec2 atlas_size(packed->width, packed->height);
    const glm::uvec2 padded_size = atlas_size + 2 * options.padding;
    const std::span<const xatlas::Vertex> vertices(mesh.vertexArray, mesh.vertexCount);

    Atlas atlas;
    atlas.size = padded_size;
    atlas.chart_count = mesh.chartCount;

    atlas.uvs.reserve(vertices.size());
    atlas.vertex_sources.reserve(vertices.size());
    for (const xatlas::Vertex &vertex : vertices) {
        const glm::dvec2 texel = glm::dvec2(vertex.uv[0], vertex.uv[1]) + glm::dvec2(options.padding);
        atlas.uvs.push_back(texel / glm::dvec2(padded_size));
        atlas.vertex_sources.push_back(vertex.xref);
    }

    const std::span<const glm::uvec3> indices = unflatten<3>(std::span<const uint32_t>(mesh.indexArray, mesh.indexCount));
    atlas.triangles.assign(indices.begin(), indices.end());

    for (const auto [triangle_index, triangle] : enumerate(atlas.triangles)) {
        if (vertices[triangle.x].chartIndex < 0) {
            atlas.unmapped_triangles.push_back(triangle_index);
        }
    }

    return atlas;
}

} // namespace uv

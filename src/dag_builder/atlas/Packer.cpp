#include <span>
#include <vector>
#include <memory>

#include <xatlas/xatlas.h>
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>

#include "log.h"
#include "Packer.h"
#include "SegmentedBuffer.h"
#include "vector_utils.h"
#include "range_utils.h"

namespace atlas {

inline void normalize_uvs(std::span<glm::dvec2> uvs) {
    if (uvs.empty()) {
        return;
    }

    glm::dvec2 min_uv = uvs[0];
    glm::dvec2 max_uv = uvs[0];
    for (const glm::dvec2 &uv : uvs) {
        min_uv = glm::min(min_uv, uv);
        max_uv = glm::max(max_uv, uv);
    }
    if (min_uv == max_uv) {
        return;
    }

    const glm::dvec2 range = max_uv - min_uv;
    for (glm::dvec2 &uv : uvs) {
        uv = (uv - min_uv) / range;
        uv = glm::clamp(uv, glm::dvec2(0.0), glm::dvec2(1.0));
    }
}

struct Packer::Impl {
    std::unique_ptr<xatlas::Atlas, decltype(&xatlas::Destroy)> atlas;

    Impl() : atlas(xatlas::Create(), xatlas::Destroy) {}

    void add_uv_mesh(const UvMesh& mesh) {
        std::vector<glm::vec2> scaled_uvs = transform_vector(mesh.uvs, [&](const glm::dvec2 &uv) {
            const glm::dvec2 scaled = uv * glm::dvec2(mesh.texture_size);
            return glm::vec2(scaled);
        });

        xatlas::UvMeshDecl meshDecl;
        meshDecl.vertexCount = scaled_uvs.size();
        meshDecl.vertexUvData = scaled_uvs.data();
        meshDecl.vertexStride = sizeof(glm::vec2);
        meshDecl.indexCount = mesh.triangles.size() * 3;
        meshDecl.indexData = mesh.triangles.data();
        meshDecl.indexFormat = xatlas::IndexFormat::UInt32;

        xatlas::AddMeshError error = xatlas::AddUvMesh(atlas.get(), meshDecl);
        if (error != xatlas::AddMeshError::Success) {
            LOG_ERROR("Error adding uv mesh for packing due to {}", xatlas::StringForEnum(error));
        }
    }

    Packing pack() {
        xatlas::ComputeCharts(this->atlas.get());
        xatlas::PackCharts(this->atlas.get());

        LOG_TRACE("Computed atlas for {} meshes", this->atlas->meshCount);

        DEBUG_ASSERT(this->atlas->atlasCount == 1);

        SegmentedBuffer<glm::dvec2> uvs;
        const std::span<const xatlas::Mesh> meshes(atlas->meshes, atlas->meshCount);
        const size_t vertex_count = sum(meshes, [&](const xatlas::Mesh &mesh) {
            return mesh.vertexCount;
        });
        uvs.reserve(vertex_count);

        for (uint32_t mesh_index = 0; mesh_index < meshes.size(); mesh_index++) {
            const xatlas::Mesh &mesh = meshes[mesh_index];
            uvs.push_new_segment(mesh.vertexCount);
            const std::span<const xatlas::Vertex> vertices(mesh.vertexArray, mesh.vertexCount);
            for (const xatlas::Vertex &vertex : vertices) {
                const glm::dvec2 uv(vertex.uv[0], vertex.uv[1]);
                uvs.last_segment()[vertex.xref] = uv;
            }
        }

        normalize_uvs(uvs.flat());

        const glm::dvec2 atlas_size(this->atlas->width, this->atlas->height);
        const double aspect = atlas_size.y > 0 ? atlas_size.x / atlas_size.y : 1.0;

        return Packing(uvs, this->atlas->utilization[0], aspect);
    }
};

Packer::Packer() {
    this->_impl = std::make_unique<Packer::Impl>();
}

Packer::~Packer() = default;

void Packer::add_uv_mesh(const UvMesh& mesh) const {
    this->_impl->add_uv_mesh(mesh);
}

Packing Packer::pack() const {
    return this->_impl->pack();
}

} // namespace atlas

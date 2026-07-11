#pragma once

#include <span>

#include <glm/glm.hpp>

#include "SegmentedBuffer.h"

namespace atlas {

struct UvMesh {
    std::span<const glm::uvec3> triangles;
    std::span<const glm::dvec2> uvs;
    glm::uvec2 texture_size;
};

class Packing {
public:
    Packing() = default;
    Packing(std::vector<glm::dvec2> uvs) : Packing(SegmentedBuffer<glm::dvec2>(std::move(uvs))) {}
    Packing(SegmentedBuffer<glm::dvec2> uvs) : _uvs(std::move(uvs)) {}

    std::span<const glm::dvec2> uvs_for_mesh(const uint32_t mesh_index) const {
        return this->_uvs.segment(mesh_index);
    }

    std::vector<glm::dvec2>& uvs() {
        return this->_uvs.backing();
    }
    const std::vector<glm::dvec2> &uvs() const {
        return this->_uvs.backing();
    }

private:
    SegmentedBuffer<glm::dvec2> _uvs;
};

class Packer {
public:
    Packer();
    ~Packer();

    void add_uv_mesh(const UvMesh &mesh) const;
    void add_uv_mesh(const std::span<const glm::uvec3> triangles,
                     const std::span<const glm::dvec2> uvs,
                     const glm::uvec2 texture_size) const {
        return this->add_uv_mesh({triangles, uvs, texture_size});
    }
    Packing pack() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;

};

} // namespace atlas

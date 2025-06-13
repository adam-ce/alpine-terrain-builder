#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>
#include <radix/geometry.h>
#include <zpp_bits.h>

class SimpleMesh {
public:
    using Triangle = glm::uvec3;
    using Edge = glm::uvec2;
    using Position = glm::dvec3;
    using Uv = glm::dvec2;
    using Texture = cv::Mat;
    using serialize = zpp::bits::members<4>;

    SimpleMesh(std::vector<Triangle> triangles, std::vector<Position> positions) 
        : SimpleMesh(triangles, positions, {}) {}
    SimpleMesh(std::vector<Triangle> triangles, std::vector<Position> positions, std::vector<Uv> uvs) 
        : SimpleMesh(triangles, positions, uvs, std::nullopt) {}
    SimpleMesh(std::vector<Triangle> triangles, std::vector<Position> positions, std::vector<Uv> uvs, Texture texture)
        : SimpleMesh(triangles, positions, uvs, std::optional<Texture>(std::move(texture))) {}
    SimpleMesh(std::vector<Triangle> triangles, std::vector<Position> positions, std::vector<Uv> uvs, std::optional<Texture> texture)
        : triangles(triangles), positions(positions), uvs(uvs), texture(std::move(texture)) {}
    SimpleMesh() = default;
    SimpleMesh(SimpleMesh &&) = default;
    SimpleMesh &operator=(SimpleMesh &&) = default;
    SimpleMesh(const SimpleMesh &) = default;
    SimpleMesh &operator=(const SimpleMesh &) = default;

    std::vector<Triangle> triangles;
    std::vector<Position> positions;
    std::vector<Uv> uvs;
    std::optional<Texture> texture;

    size_t vertex_count() const {
        return this->positions.size();
    }
    size_t face_count() const {
        return this->triangles.size();
    }
    bool is_empty() const {
        return this->vertex_count() == 0 && this->face_count() == 0;
    }

    bool has_uvs() const {
        return this->uvs.size() > 0;
    }
    bool has_texture() const {
        return this->texture.has_value();
    }
};

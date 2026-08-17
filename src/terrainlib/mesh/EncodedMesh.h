#pragma once

#include <vector>

#include <glm/glm.hpp>
namespace mesh {

class Encoded {
public:
    struct Header {
        uint32_t version;
        uint32_t n_dims;
        uint32_t component_type;
        uint32_t vertex_count;
        uint32_t face_count;

        bool operator==(const Header &) const = default;
        bool operator!=(const Header &) const = default;
    };

    Header header;
    std::vector<uint8_t> triangles;
    std::vector<uint8_t> positions;
    std::vector<uint8_t> uvs;
    std::vector<uint8_t> texture;
};

} // namespace mesh

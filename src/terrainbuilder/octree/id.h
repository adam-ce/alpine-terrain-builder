#pragma once

#include <array>
#include <cstdint>
#include <fmt/core.h>
#include <fmt/format.h>
#include <glm/glm.hpp>
#include <optional>
#include <stdexcept>

namespace octree {

class Id {
public:
    constexpr Id(uint32_t level, glm::uvec3 coords)
        : _level(level), _index(interleave3(coords)) {}
    constexpr Id(uint32_t level, uint64_t index)
        : _level(level), _index(index) {}

    constexpr uint32_t level() const {
        return this->_level;
    }
    constexpr uint64_t index_on_level() const {
        return this->_index;
    }
    constexpr glm::uvec3 coords() const {
        return deinterleave3(this->index_on_level());
    }
    constexpr uint32_t x() const {
        return this->coords().x;
    }
    constexpr uint32_t y() const {
        return this->coords().y;
    }
    constexpr uint32_t z() const {
        return this->coords().z;
    }

    constexpr std::optional<Id> neighbour(const glm::ivec3& d) const {
        const auto new_coords = glm::ivec3(this->coords()) + d;
        if (new_coords.x < 0 || new_coords.y < 0 || new_coords.z < 0) {
            // throw std::out_of_range("Neighbour coordinates cannot be negative");
            return std::nullopt;
        }
        return Id(this->level(), glm::uvec3(new_coords));
    }

    constexpr std::vector<Id> neighbours() const {
        std::vector<Id> result;
        result.reserve(26);
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    if (dx == 0 && dy == 0 && dz == 0) {
                        continue; // Skip the center itself
                    }

                    const glm::ivec3 offset(dx, dy, dz);
                    const auto neighbour = this->neighbour({dx, dy, dz});
                    if (!neighbour.has_value()) {
                        continue;
                    }
                    result.push_back(neighbour.value());
                }
            }
        }
        return result;
    }

    constexpr std::optional<Id> parent() const {
        if (this->level() == 0) {
            return std::nullopt;
        }

        const auto parent_coords = this->coords() / glm::uvec3(2);
        return Id(this->level() - 1, parent_coords);
    }

    constexpr Id child(uint32_t child_index) const {
        if (child_index > 7) {
            throw std::invalid_argument("Invalid child index (must be 0-7)");
        }
        return Id(this->level() + 1, (this->index_on_level() << 3) | child_index);
    }

    constexpr std::array<Id, 8> children() const {
        return {
            this->child(0), this->child(1), this->child(2), this->child(3), 
            this->child(4), this->child(5), this->child(6), this->child(7)
        };
    }

    static constexpr Id root() {
        return Id(0, 0);
    }

    bool operator==(const Id &other) const {
        return this->_level == other._level && this->_index == other._index;
    }

private:
    uint32_t _level;
    uint64_t _index;

    static constexpr uint64_t interleave3(const glm::uvec3 &coords) {
        const uint64_t x = coords.x;
        const uint64_t y = coords.y;
        const uint64_t z = coords.z;

        uint64_t result = 0;
        for (uint32_t i = 0; i < (sizeof(uint64_t) * 8) / 3; i++) {
            result |= ((x >> i) & 1) << (3 * i);
            result |= ((y >> i) & 1) << (3 * i + 1);
            result |= ((z >> i) & 1) << (3 * i + 2);
        }
        return result;
    }

    static constexpr glm::uvec3 deinterleave3(uint64_t index) {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t z = 0;
        for (uint32_t i = 0; i < (sizeof(uint64_t) * 8) / 3; i++) {
            x |= ((index >> (3 * i)) & 1) << i;
            y |= ((index >> (3 * i + 1)) & 1) << i;
            z |= ((index >> (3 * i + 2)) & 1) << i;
        }
        return {x, y, z};
    }
};

} // namespace octree

template <>
struct fmt::formatter<octree::Id> {
    // Parses format specifications; here we ignore them.
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    // Format the Id object.
    template <typename FormatContext>
    auto format(const octree::Id &id, FormatContext &ctx) {
        return fmt::format_to(
            ctx.out(),
            "Id(level={}, coords=({}, {}, {}), index={})",
            id.level(), id.x(), id.y(), id.z(), id.index_on_level());
    }
};
#include <fmt/ostream.h>
#include <iostream>
namespace octree{
inline std::string to_string(const octree::Id &id) {
    return fmt::format("{}", id);
}
inline std::ostream &operator<<(std::ostream &os, const octree::Id &id) {
    fmt::print(os, "{}", id);
    return os;
}
}

#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <functional>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "hash_utils.h"
#include "int_math.h"

namespace octree {

class Id {
public:
    using Level = uint8_t;
    using Coord = uint32_t;
    using Coords = glm::tvec3<Coord>;
    using Index = uint64_t;

    [[nodiscard]] static constexpr Level max_level() {
        return (sizeof(Index) * 8) / 3;
    }
    [[nodiscard]] static constexpr Coord max_coord_on_level(const Level level) {
        return (1ull << level) - 1;
    }
    [[nodiscard]] static constexpr Index max_index_on_level(const Level level) {
        return (1ull << (3 * level)) - 1;
    }

    constexpr Id() = default; // zpp::bits requires a default constructor
    constexpr Id(const Level level, const Coords coords)
        : Id(level, interleave3(coords)) {
    }
    constexpr Id(const Level level, const Index index)
        : _level(level), _index(index) {
        DEBUG_ASSERT(level <= Id::max_level());
        DEBUG_ASSERT(index <= Id::max_index_on_level(this->_level));
    }

    [[nodiscard]] static std::optional<Id> try_make(const Level level, const Coords coords) {
        if (level > Id::max_level()) {
            return std::nullopt;
        }
        if (glm::any(glm::greaterThan(coords, Coords(max_coord_on_level(level))))) {
            return std::nullopt;
        }
        return Id(level, coords);
    }
    [[nodiscard]] static std::optional<Id> try_make(const Level level, const Index index) {
        if (level > Id::max_level()) {
            return std::nullopt;
        }
        if (index > Id::max_index_on_level(level)) {
            return std::nullopt;
        }
        return Id(level, index);
    }

    [[nodiscard]] constexpr Level level() const {
        return this->_level;
    }
    [[nodiscard]] constexpr Index index_on_level() const {
        return this->_index;
    }
    [[nodiscard]] constexpr Coords coords() const {
        return deinterleave3(this->index_on_level());
    }
    [[nodiscard]] constexpr Coord x() const {
        return this->coords().x;
    }
    [[nodiscard]] constexpr Coord y() const {
        return this->coords().y;
    }
    [[nodiscard]] constexpr Coord z() const {
        return this->coords().z;
    }

    [[nodiscard]] constexpr std::optional<Id> neighbour(const glm::ivec3& d) const {
        const Coord max_coord = Id::max_coord_on_level(this->level());
        const glm::ivec3 new_coords = glm::ivec3(this->coords()) + d;
        if (glm::any(glm::lessThan(new_coords, glm::ivec3(0))) || glm::any(glm::greaterThan(new_coords, glm::ivec3(max_coord)))) {
            return std::nullopt;
        }
        return Id(this->level(), Coords(new_coords));
    }

    [[nodiscard]] std::vector<Id> neighbours() const {
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

    [[nodiscard]] constexpr std::optional<Id> parent() const {
        if (this->level() == 0) {
            return std::nullopt;
        }

        const auto parent_coords = this->coords() / Coords(2);
        return Id(this->level() - 1, parent_coords);
    }

    [[nodiscard]] constexpr std::optional<Id> child(uint32_t child_index) const {
        if (child_index > 7) {
            throw std::invalid_argument("Invalid child index (must be 0-7)");
        }
        if (!this->has_children()) {
            return std::nullopt;
        }
        return this->_child(child_index);
    }

    [[nodiscard]] constexpr std::optional<std::array<Id, 8>> children() const {
        if (!this->has_children()) {
            return std::nullopt;
        }
        return std::array<Id, 8>{
            this->_child(0), this->_child(1), this->_child(2), this->_child(3),
            this->_child(4), this->_child(5), this->_child(6), this->_child(7)};
    }

    [[nodiscard]] constexpr bool has_children() const {
        return this->level() < Id::max_level();
    }

    [[nodiscard]] constexpr bool is_root() const {
        return this->level() == 0;
    }

    [[nodiscard]] static constexpr Id root() {
        return Id(0, 0);
    }

    [[nodiscard]] constexpr std::optional<Id> ancestor_on_level(const Level target_level) const {
        if (target_level > this->level()) {
            return std::nullopt;
        }
        const Level level_diff = this->level() - target_level;
        const auto ancestor_coords = this->coords() / ipow2<Coord>(level_diff);
        return Id(target_level, ancestor_coords);
    }
    [[nodiscard]] std::vector<Id> descendants_on_level(const Level target_level) const {
        if (target_level < this->level()) {
            return {};
        }

        const Level level_diff = target_level - this->level();

        const Index count = ipow2<Index>(level_diff * 3u);
        const Index first_index = this->index_on_level() * count;

        std::vector<Id> descendants;
        descendants.reserve(count);

        for (Index offset = 0; offset < count; offset++) {
            descendants.emplace_back(target_level, first_index | offset);
        }

        return descendants;
    }
    [[nodiscard]] constexpr bool is_descendant_of(const Id &other, const bool include_self = false) const {
        return other.is_ancestor_of(*this, include_self);
    }

    [[nodiscard]] constexpr bool is_ancestor_of(const Id &other, const bool include_self = false) const {
        if (this->is_root()) {
            return true;
        }
        if (include_self && *this == other) {
            return true;
        }
        if (this->level() >= other.level()) {
            return false;
        }
        return other.ancestor_on_level(this->level()) == *this;
    }

    constexpr bool operator==(const Id &other) const {
        return this->_level == other._level && this->_index == other._index;
    }
    constexpr bool operator!=(const Id &other) const {
        return !(*this == other);
    }
    constexpr std::strong_ordering operator<=>(const Id &other) const {
        if (this->_level < other._level) {
            return std::strong_ordering::less;
        }
        if (this->_level > other._level) {
            return std::strong_ordering::greater;
        }
        return this->_index <=> other._index;
    }

    std::string to_string() const;

private:
    Level _level;
    Index _index;

    [[nodiscard]] constexpr Id _child(const uint32_t child_index) const {
        DEBUG_ASSERT(child_index <= 7);
        return Id(this->level() + 1, (this->index_on_level() << 3) | child_index);
    }

    [[nodiscard]] static constexpr Index interleave3(const Coords &coords) {
        DEBUG_ASSERT(glm::all(glm::lessThanEqual(coords, Coords(Id::max_coord_on_level(Id::max_level())))));

        const Index x = coords.x;
        const Index y = coords.y;
        const Index z = coords.z;

        Index result = 0;
        for (Level i = 0; i < Id::max_level(); i++) {
            result |= ((x >> i) & 1) << (3 * i);
            result |= ((y >> i) & 1) << (3 * i + 1);
            result |= ((z >> i) & 1) << (3 * i + 2);
        }
        return result;
    }

    [[nodiscard]] static constexpr Coords deinterleave3(Index index) {
        Coord x = 0;
        Coord y = 0;
        Coord z = 0;
        for (Level i = 0; i < Id::max_level(); i++) {
            x |= ((index >> (3 * i)) & 1) << i;
            y |= ((index >> (3 * i + 1)) & 1) << i;
            z |= ((index >> (3 * i + 2)) & 1) << i;
        }
        return {x, y, z};
    }
};

} // namespace octree

#include <fmt/format.h>
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
namespace octree {
inline std::string octree::Id::to_string() const {
    return fmt::format("{}", *this);
}
inline std::ostream &operator<<(std::ostream &os, const octree::Id &id) {
    fmt::print(os, "{}", id);
    return os;
}
}

namespace std {
template <>
struct hash<octree::Id> {
    std::size_t operator()(const octree::Id &id) const noexcept {
        return ::hash::combine(id.level(), id.index_on_level());
    }
};
} // namespace std

#include <zpp_bits.h>
namespace zpp::bits {
namespace {
constexpr zpp::bits::errc success() {
    return zpp::bits::errc(std::errc());
}
}
constexpr auto serialize(auto &archive, octree::Id &id) {
    octree::Id::Level level;
    octree::Id::Index index;
    auto result = archive(level, index);
    if (failure(result)) {
        return result;
    }

    auto maybe_id = octree::Id::try_make(level, index);
    if (!maybe_id) {
        return zpp::bits::errc(std::errc::bad_message);
    }

    id = *maybe_id;
    return success();
}
constexpr auto serialize(auto &archive, const octree::Id &id) {
    return archive(id.level(), id.index_on_level());
}
}

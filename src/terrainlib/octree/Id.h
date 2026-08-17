#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <functional>
#include <type_traits>

#include <fmt/format.h>
#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "numeric/int_math.h"
#include "numeric/number_types.h"

namespace octree {

namespace detail {
template <typename T>
[[nodiscard]] inline constexpr T low_mask(const size_t bit_count) {
    if (bit_count == 0) {
        return T{0};
    }
    if (bit_count >= std::numeric_limits<T>::digits) {
        return (std::numeric_limits<T>::max)();
    }
    return (T{1} << bit_count) - T{1};
}

template <typename IndexT, size_t Dimensions, size_t Dimension = 0, typename Function>
static constexpr void for_each_nd_index_impl(
    const glm::vec<Dimensions, IndexT> &extents,
    glm::vec<Dimensions, IndexT> &index,
    Function &function) {
    if constexpr (Dimension == Dimensions) {
        function(index);
    } else {
        for (index[Dimension] = 0; index[Dimension] < extents[Dimension]; index[Dimension]++) {
            for_each_nd_index_impl<IndexT, Dimensions, Dimension + 1, Function>(extents, index, function);
        }
    }
}

template <typename IndexT, size_t Dimensions, typename Function>
inline constexpr void for_each_nd_index(const glm::vec<Dimensions, IndexT> &extents, Function &&function) {
    glm::vec<Dimensions, IndexT> index{};
    for_each_nd_index_impl<IndexT, Dimensions, 0, Function>(extents, index, function);
}

template <typename IndexT, size_t Dimensions, typename Function>
inline constexpr void for_each_nd_index(const IndexT extent, Function &&function) {
    const glm::vec<Dimensions, IndexT> extents(extent);
    for_each_nd_index<IndexT, Dimensions, Function>(extents, std::forward<Function>(function));
}
}

template <
    size_t Dimensions,
    size_t MaxLevel,
    typename LevelT = uint_for_bits_t<std::bit_width(MaxLevel)>,
    typename IndexT = uint_for_bits_t<Dimensions * MaxLevel>,
    typename CoordT = uint_for_bits_t<MaxLevel>>
class Id_ {
public:
    using Level = LevelT;
    using Index = IndexT;
    using Coord = CoordT;
    using Coords = glm::vec<Dimensions, Coord>;
    using SignedCoord = int_for_bits_t<MaxLevel + 1>;
    using Offset = glm::vec<Dimensions, SignedCoord>;

    static_assert(Dimensions > 0);
    static_assert(std::numeric_limits<Level>::is_integer && !std::numeric_limits<Level>::is_signed);
    static_assert(std::numeric_limits<Index>::is_integer && !std::numeric_limits<Index>::is_signed);
    static_assert(std::numeric_limits<Coord>::is_integer && !std::numeric_limits<Coord>::is_signed);
    static_assert(std::numeric_limits<Level>::digits >= std::bit_width(MaxLevel));
    static_assert(std::numeric_limits<Index>::digits >= Dimensions * MaxLevel);
    static_assert(std::numeric_limits<Coord>::digits >= MaxLevel);

    static constexpr size_t child_count = ipow(2, Dimensions);
    static constexpr size_t neighbour_count = ipow(3, Dimensions) - 1;

    [[nodiscard]] static constexpr size_t dimensions() {
        return Dimensions;
    }
    [[nodiscard]] static constexpr Level max_level() {
        return static_cast<Level>(MaxLevel);
    }
    [[nodiscard]] static constexpr Index max_index_on_level(const Level level) {
        return detail::low_mask<Index>(Dimensions * static_cast<size_t>(level));
    }
    [[nodiscard]] static constexpr Coord max_coord_on_level(const Level level) {
        return detail::low_mask<Coord>(static_cast<size_t>(level));
    }
    [[nodiscard]] static constexpr Coords max_coords_on_level(const Level level) {
        return Coords(max_coord_on_level(level));
    }

    constexpr Id_() = default;
    template <typename... Coordinates>
        requires(sizeof...(Coordinates) == Dimensions && (std::convertible_to<Coordinates, Coord> && ...))
    constexpr Id_(const Level level, Coordinates... coordinates) : Id_(level, Coords{coordinates...}) {}
    constexpr Id_(const Level level, const Coords coords) : Id_(level, interleave(coords)) {}
    constexpr Id_(const Level level, const Index index) : _level(level), _index(index) {
        DEBUG_ASSERT(level <= Id_::max_level());
        DEBUG_ASSERT(index <= Id_::max_index_on_level(level));
    }

    [[nodiscard]] static std::optional<Id_> try_make(const Level level, const Coords coords) {
        if (level > Id_::max_level()) {
            return std::nullopt;
        }
        if (glm::any(glm::greaterThan(coords, Id_::max_coords_on_level(level)))) {
            return std::nullopt;
        }
        return Id_(level, coords);
    }
    [[nodiscard]] static std::optional<Id_> try_make(const Level level, const Index index) {
        if (level > Id_::max_level()) {
            return std::nullopt;
        }
        if (index > Id_::max_index_on_level(level)) {
            return std::nullopt;
        }
        return Id_(level, index);
    }

    [[nodiscard]] constexpr Level level() const {
        return this->_level;
    }
    [[nodiscard]] constexpr Index index_on_level() const {
        return this->_index;
    }
    [[nodiscard]] constexpr Coords coords() const {
        return deinterleave(this->index_on_level());
    }
    [[nodiscard]] constexpr Coord x() const requires (Dimensions >= 1) {
        return this->coords().x;
    }
    [[nodiscard]] constexpr Coord y() const requires (Dimensions >= 2) {
        return this->coords().y;
    }
    [[nodiscard]] constexpr Coord z() const requires (Dimensions >= 3) {
        return this->coords().z;
    }

    [[nodiscard]] constexpr std::optional<Id_> neighbour(const Offset &d) const {
        const Offset new_coords = Offset(this->coords()) + d;
        const Offset max_coords = Id_::max_coords_on_level(this->level());
        if (glm::any(glm::lessThan(new_coords, Offset(0))) || glm::any(glm::greaterThan(new_coords, max_coords))) {
            return std::nullopt;
        }
        return Id_(this->level(), Coords(new_coords));
    }
    [[nodiscard]] constexpr std::optional<Id_> prev() const {
        const Level level = this->level();
        const Index index = this->index_on_level();
        const Index min_index = 0;
        if (index == min_index) {
            return std::nullopt;
        }
        return Id_(level, index-1);
    }
    [[nodiscard]] constexpr std::optional<Id_> next() const {
        const Level level = this->level();
        const Index index = this->index_on_level();
        const Index max_index = Id_::max_index_on_level(level);
        if (index == max_index) {
            return std::nullopt;
        }
        return Id_(level, index+1);
    }

    [[nodiscard]] std::vector<Id_> neighbours() const {
        std::vector<Id_> result;
        result.reserve(neighbour_count);

        detail::for_each_nd_index<SignedCoord, Dimensions>(3, [&](const Offset &index) {
            const Offset offset = index - SignedCoord{1};
            if (offset == Offset{0}) {
                return;
            }

            const auto candidate = this->neighbour(offset);
            if (candidate.has_value()) {
                result.push_back(candidate.value());
            }
        });

        return result;
    }

    [[nodiscard]] constexpr std::optional<Id_> parent() const {
        if (this->level() == 0) {
            return std::nullopt;
        }

        const auto parent_coords = this->coords() / Coords(2);
        return Id_(this->level() - 1, parent_coords);
    }

    [[nodiscard]] constexpr std::optional<Id_> child(size_t child_index) const {
        if (child_index >= child_count) {
            throw std::invalid_argument(fmt::format("Invalid child index (must be 0-{})", child_count));
        }
        if (!this->has_children()) {
            return std::nullopt;
        }
        return this->_child(child_index);
    }

    [[nodiscard]] constexpr std::optional<std::array<Id_, child_count>> children() const {
        if (!this->has_children()) {
            return std::nullopt;
        }
        return [&]<size_t... ChildIndices>(std::index_sequence<ChildIndices...>) {
            return std::array<Id_, child_count>{this->_child(ChildIndices)...};
        }(std::make_index_sequence<child_count>{});
    }

    [[nodiscard]] constexpr bool has_children() const {
        return this->level() < Id_::max_level();
    }

    [[nodiscard]] constexpr bool is_root() const {
        return this->level() == 0;
    }

    [[nodiscard]] static constexpr Id_ root() {
        return Id_(0, 0);
    }

    [[nodiscard]] constexpr std::optional<Id_> ancestor_on_level(const Level target_level) const {
        if (target_level > this->level()) {
            return std::nullopt;
        }
        const Level level_diff = this->level() - target_level;
        const Coords ancestor_coords = this->coords() / ipow2<Coord>(level_diff);
        return Id_(target_level, ancestor_coords);
    }
    [[nodiscard]] std::vector<Id_> descendants_on_level(const Level target_level) const {
        if (target_level < this->level()) {
            return {};
        }

        const Level level_diff = target_level - this->level();

        const Index count = ipow2<Index>(level_diff * Dimensions);
        const Index first_index = this->index_on_level() * count;

        std::vector<Id_> descendants;
        descendants.reserve(count);

        for (Index offset = 0; offset < count; offset++) {
            descendants.emplace_back(target_level, first_index | offset);
        }

        return descendants;
    }
    [[nodiscard]] constexpr bool is_descendant_of(const Id_ &other, const bool include_self = false) const {
        return other.is_ancestor_of(*this, include_self);
    }

    [[nodiscard]] constexpr bool is_ancestor_of(const Id_ &other, const bool include_self = false) const {
        if (!include_self && *this == other) {
            return false;
        }
        if (this->is_root()) {
            return true;
        }
        if (this->level() >= other.level()) {
            return false;
        }
        return other.ancestor_on_level(this->level()) == *this;
    }

    constexpr bool operator==(const Id_ &other) const {
        return this->_level == other._level && this->_index == other._index;
    }
    constexpr bool operator!=(const Id_ &other) const {
        return !(*this == other);
    }
    constexpr std::strong_ordering operator<=>(const Id_ &other) const {
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

    [[nodiscard]] constexpr Id_ _child(const size_t child_index) const {
        DEBUG_ASSERT(child_index < child_count);
        return Id_(
            this->level() + 1,
            (this->index_on_level() << Dimensions) | static_cast<Index>(child_index));
    }

    [[nodiscard]] static constexpr Index interleave(const Coords &coords) {
        DEBUG_ASSERT(glm::all(glm::lessThanEqual(coords, Id_::max_coords_on_level(Id_::max_level()))));

        Index result{};

        for (size_t bit = 0; bit < MaxLevel; bit++) {
            for (size_t dimension = 0; dimension < Dimensions; dimension++) {
                result |= static_cast<Index>((coords[dimension] >> bit) & Coord{1}) << (Dimensions * bit + dimension);
            }
        }
        return result;
    }

    [[nodiscard]] static constexpr Coords deinterleave(Index index) {
        Coords result{};
        for (size_t bit = 0; bit < MaxLevel; bit++) {
            for (size_t dimension = 0; dimension < Dimensions; dimension++) {
                result[dimension] |= static_cast<Coord>((index >> (Dimensions * bit + dimension)) & Index{1}) << bit;
            }
        }
        return result;
    }
};

using Id = Id_<3, 21>;

} // namespace octree

template <
    size_t Dimensions,
    size_t MaxLevel,
    typename LevelT,
    typename IndexT,
    typename CoordT>
struct fmt::formatter<octree::Id_<Dimensions, MaxLevel, LevelT, IndexT, CoordT>> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const octree::Id_<Dimensions, MaxLevel, LevelT, IndexT, CoordT> &id, FormatContext &ctx) const {
        auto out = fmt::format_to(ctx.out(), "Id(level={}, coords=(", id.level());

        const auto coords = id.coords();
        for (size_t dimension = 0; dimension < Dimensions; dimension++) {
            if (dimension != 0) {
                out = fmt::format_to(out, ", ");
            }
            out = fmt::format_to(out, "{}", coords[dimension]);
        }

        return fmt::format_to(out, "), index={})", id.index_on_level());
    }
};

#include <fmt/ostream.h>
#include <iostream>
namespace octree {
template <
    size_t Dimensions,
    size_t MaxLevel,
    typename LevelT,
    typename IndexT,
    typename CoordT>
inline std::string Id_<Dimensions, MaxLevel, LevelT, IndexT, CoordT>::to_string() const {
    return fmt::format("{}", *this);
}
template <
    size_t Dimensions,
    size_t MaxLevel,
    typename LevelT,
    typename IndexT,
    typename CoordT>
inline std::ostream &operator<<(std::ostream &os, const Id_<Dimensions, MaxLevel, LevelT, IndexT, CoordT> &id) {
    fmt::print(os, "{}", id);
    return os;
}
} // namespace octree

#include "hash_utils.h"
namespace std {
template <
    size_t Dimensions,
    size_t MaxLevel,
    typename LevelT,
    typename IndexT,
    typename CoordT>
struct hash<octree::Id_<Dimensions, MaxLevel, LevelT, IndexT, CoordT>> {
    size_t operator()(const octree::Id_<Dimensions, MaxLevel, LevelT, IndexT, CoordT> &id) const noexcept {
        return ::hash::combine(id.level(), id.index_on_level());
    }
};
} // namespace std

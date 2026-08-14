#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <optional>

#include <radix/tile.h>

namespace raster_store {

struct StoreTraits {
    using Key = radix::tile::Id;
    using Hasher = Key::Hasher;

    static constexpr unsigned max_zoom_level = std::numeric_limits<uint32_t>::digits;

    static Key root() {
        return {0, {0, 0}};
    }
    static std::optional<Key> parent(const Key &key) {
        if (key.zoom_level == 0) {
            return std::nullopt;
        }
        return key.parent();
    }
    static std::optional<std::array<Key, 4>> children(const Key &key) {
        if (key.zoom_level >= max_zoom_level) {
            return std::nullopt;
        }
        return key.children();
    }
    static bool is_valid(const Key &key) {
        if (key.zoom_level > max_zoom_level) {
            return false;
        }
        if (key.zoom_level == max_zoom_level) {
            return true;
        }
        const uint32_t extent = uint32_t{1} << key.zoom_level;
        return key.coords.x < extent && key.coords.y < extent;
    }
};

} // namespace raster_store

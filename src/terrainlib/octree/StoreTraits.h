#pragma once

#include <functional>

#include "octree/Id.h"

namespace octree {

struct StoreTraits {
    using Key = Id;
    using Hasher = std::hash<Key>;

    static constexpr Key root() { return Key::root(); }
    static constexpr auto parent(const Key& key) { return key.parent(); }
    static constexpr auto children(const Key& key) { return key.children(); }
    static constexpr bool is_valid(const Key& key) { return key.level() <= Key::max_level() && key.index_on_level() <= Key::max_index_on_level(key.level()); }
};

} // namespace octree

#pragma once

#include "Error.h"
#include "raster_store/StoreTraits.h"
#include "store/Index.h"
#include <optional>
#include <vector>

namespace rf_merger::partition {

using Key = radix::tile::Id;
using Index = store::Index<raster_store::StoreTraits>;

// An output leaf with the original physical tile of each input covering it.
// Suppliers are never finer than the leaf.
struct Leaf {
    Key key;
    std::optional<Key> left;
    std::optional<Key> right;

    bool operator==(const Leaf&) const = default;
};

// The physical tile covering key at the same or a coarser zoom level.
Expected<std::optional<Key>> supplier(const Index& index, const Key& key);

// Whether key is a leaf of the output partition, derived from both indices.
Expected<bool> is_leaf(const Index& left, const Index& right, const Key& key);

// Lazily enumerates output leaves in depth-first order from index topology
// alone: wherever either input has a physical leaf, the output is at least
// that fine. Retains only the traversal frontier.
class Cursor {
public:
    Cursor(const Index& left, const Index& right);

    Expected<std::optional<Leaf>> next();

private:
    struct Frame {
        Key key;
        std::optional<Key> left;
        std::optional<Key> right;
    };
    const Index* m_left;
    const Index* m_right;
    std::vector<Frame> m_stack;
};

} // namespace rf_merger::partition

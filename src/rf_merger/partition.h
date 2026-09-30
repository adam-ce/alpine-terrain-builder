/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#pragma once

#include "raster_store/StoreTraits.h"
#include "store/Index.h"
#include <optional>
#include <vector>

// Indices must be validated and free of physical Inner nodes; the merger
// rejects such inputs before partitioning.
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
std::optional<Key> supplier(const Index& index, const Key& key);

// Whether key is a leaf of the output partition, derived from both indices.
bool is_leaf(const Index& left, const Index& right, const Key& key);

// Lazily enumerates output leaves in depth-first order from index topology
// alone: wherever either input has a physical leaf, the output is at least
// that fine. Retains only the traversal frontier.
class Cursor {
public:
    Cursor(const Index& left, const Index& right);

    std::optional<Leaf> next();

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

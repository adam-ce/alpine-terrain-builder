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

#include "partition.h"

#include <libassert/assert.hpp>

namespace rf_merger::partition {
namespace {
    std::optional<store::NodeStatus> status(const Index& index, const Key& key)
    {
        const auto result = Error::asserting_unwrap(index.get(key));
        ASSERT(result != store::NodeStatus::Inner, to_string(key));
        return result;
    }
} // namespace

std::optional<Key> supplier(const Index& index, const Key& key)
{
    for (std::optional<Key> current = key; current; current = raster_store::StoreTraits::parent(*current)) {
        if (status(index, *current) == store::NodeStatus::Leaf) {
            return current;
        }
    }
    return std::nullopt;
}

bool is_leaf(const Index& left, const Index& right, const Key& key)
{
    if (status(left, key) == store::NodeStatus::Virtual || status(right, key) == store::NodeStatus::Virtual) {
        return false;
    }
    return supplier(left, key) || supplier(right, key);
}

Cursor::Cursor(const Index& left, const Index& right)
    : m_left(&left)
    , m_right(&right)
{
    if (!left.empty() || !right.empty()) {
        m_stack.push_back({ raster_store::StoreTraits::root(), std::nullopt, std::nullopt });
    }
}

std::optional<Leaf> Cursor::next()
{
    while (!m_stack.empty()) {
        auto frame = m_stack.back();
        m_stack.pop_back();
        const auto left = status(*m_left, frame.key);
        const auto right = status(*m_right, frame.key);
        if (left == store::NodeStatus::Leaf) {
            frame.left = frame.key;
        }
        if (right == store::NodeStatus::Leaf) {
            frame.right = frame.key;
        }
        if (left != store::NodeStatus::Virtual && right != store::NodeStatus::Virtual) {
            if (frame.left || frame.right) {
                return Leaf { frame.key, frame.left, frame.right };
            }
            continue;
        }
        const auto children = raster_store::StoreTraits::children(frame.key);
        ASSERT(children.has_value(), to_string(frame.key));
        // Reverse order pops the first child next.
        for (auto child = children->rbegin(); child != children->rend(); ++child) {
            m_stack.push_back({ *child, frame.left, frame.right });
        }
    }
    return std::nullopt;
}

} // namespace rf_merger::partition

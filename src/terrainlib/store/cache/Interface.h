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

#include <optional>

#include "store/Traits.h"

namespace store::cache {

template <HierarchyTraits Traits, typename NodeData>
class Interface {
public:
    using Key = typename Traits::Key;
    virtual ~Interface() = default;

    virtual std::optional<NodeData> get(const Key& key) = 0;
    virtual bool put(const Key& key, const NodeData& value) = 0;
    virtual bool remove(const Key& key) noexcept = 0;
    virtual bool contains(const Key& key) const noexcept = 0;
};

} // namespace store::cache

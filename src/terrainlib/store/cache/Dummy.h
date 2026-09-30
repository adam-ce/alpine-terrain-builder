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

#include "store/cache/Interface.h"

namespace store::cache {

template <HierarchyTraits Traits, typename NodeData>
class Dummy final : public Interface<Traits, NodeData> {
public:
    using Key = typename Traits::Key;
    std::optional<NodeData> get(const Key&) noexcept override { return std::nullopt; }
    bool put(const Key&, const NodeData&) noexcept override { return false; }
    bool remove(const Key&) noexcept override { return false; }
    bool contains(const Key&) const noexcept override { return false; }
};

} // namespace store::cache

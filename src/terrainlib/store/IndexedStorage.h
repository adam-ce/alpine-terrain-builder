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

#include <utility>

#include "store/Storage.h"

namespace store {

template <HierarchyTraits Traits, typename NodeData>
class IndexedStorage : public Storage<Traits, NodeData> {
public:
    using Base = Storage<Traits, NodeData>;
    using typename Base::Persistence;

    explicit IndexedStorage(Storage<Traits, NodeData> storage)
        : Base(std::move(storage))
    {
        this->ensure_indexed();
    }

    IndexedStorage(RawStorage<Traits, NodeData> raw, Index<Traits> index, Persistence persistence)
        : Base(std::move(raw), std::move(index), std::move(persistence))
    {
    }

    IndexedStorage(const IndexedStorage&) = delete;
    IndexedStorage& operator=(const IndexedStorage&) = delete;
    IndexedStorage(IndexedStorage&&) noexcept = default;
    IndexedStorage& operator=(IndexedStorage&&) noexcept = default;

    const Index<Traits>& index() const { return this->index_ref(); }
};

} // namespace store

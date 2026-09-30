/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 * Copyright (C) 2025 Adam Celarek-Litofcenko
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

#include "merge/NodeData.h"
#include "merge/Result.h"
#include "octree/Id.h"
#include "store/NodeStatusOrMissing.h"

namespace merge {
template <typename T>
concept Visitor = requires(T t, const octree::Id &id) {
    // Must define a nested Context type
    typename T::Context;

    // Must provide a way to create the root context
    { t.make_root_context() } -> std::same_as<typename T::Context>;

    {
        t.template visit<store::NodeStatusOrMissing::Leaf, store::NodeStatusOrMissing::Leaf>(
            id,
            std::declval<const NodeData<store::NodeStatusOrMissing::Leaf> &>(),
            std::declval<const NodeData<store::NodeStatusOrMissing::Leaf> &>(),
            std::declval<const typename T::Context &>())
    } -> std::same_as<Result<typename T::Context>>;
};

} // namespace merge

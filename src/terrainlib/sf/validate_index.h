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

#include <expected>

#include "Error.h"
#include "octree/StoreTraits.h"
#include "store/Index.h"

namespace sf {

inline Expected<void> validate_index(const store::Index<octree::StoreTraits>& index)
{
    for (const auto& [key, status] : index) {
        if (status == store::NodeStatus::Inner) {
            return Error::fail(Error::Code::CorruptData, "Structura Fundamentalis topology contains Inner node " + key.to_string());
        }
    }
    return {};
}

} // namespace sf

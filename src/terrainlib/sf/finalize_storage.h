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

#include <libassert/assert.hpp>

#include "mesh/storage.h"
#include "sf/validate_index.h"

namespace sf {

inline Expected<void> finalize_storage(mesh::storage::Storage& storage)
{
    auto save_result = storage.save_or_create_index();
    if (!save_result) {
        return save_result;
    }

    const auto index = storage.index();
    DEBUG_ASSERT(index.has_value());
    return validate_index(index->get());
}

} // namespace sf

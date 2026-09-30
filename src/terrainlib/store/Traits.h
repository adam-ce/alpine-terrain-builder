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

#include <concepts>
#include <source_location>
#include <string>

#include "Error.h"

namespace store {

template <typename Traits>
concept HierarchyTraits = requires(typename Traits::Key key) {
    typename Traits::Key;
    typename Traits::Hasher;
    { Traits::root() } -> std::same_as<typename Traits::Key>;
    Traits::parent(key);
    Traits::children(key);
    { Traits::is_valid(key) } -> std::same_as<bool>;
    { Traits::key_to_string(key) } -> std::same_as<std::string>;
};

template <HierarchyTraits Traits>
std::unexpected<Error> invalid_key_error(const typename Traits::Key& key, const std::source_location location = std::source_location::current())
{
    return Error::fail(Error::Code::InvalidInput, "invalid hierarchy key " + Traits::key_to_string(key), location);
}

} // namespace store

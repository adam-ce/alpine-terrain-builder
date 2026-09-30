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

#include <glm/glm.hpp>
#include <zpp_bits.h>

#include <utility>

namespace io::glm_serialization {

template <typename Archive, typename Value, glm::length_t... Indices>
constexpr auto serialize_components(Archive& archive, Value& value, std::integer_sequence<glm::length_t, Indices...>)
{
    return archive(value[Indices]...);
}

} // namespace io::glm_serialization

// zpp::bits discovers these overloads through argument-dependent lookup.
namespace glm {

// Store scalar components only, excluding any alignment padding.
template <typename Archive, length_t Length, typename Scalar, qualifier Qualifier>
constexpr auto serialize(Archive& archive, vec<Length, Scalar, Qualifier>& value)
{
    return io::glm_serialization::serialize_components(archive, value, std::make_integer_sequence<length_t, Length> {});
}

template <typename Archive, length_t Length, typename Scalar, qualifier Qualifier>
constexpr auto serialize(Archive& archive, const vec<Length, Scalar, Qualifier>& value)
{
    return io::glm_serialization::serialize_components(archive, value, std::make_integer_sequence<length_t, Length> {});
}

// Matrix indexing yields columns, so the wire format is column-major.
template <typename Archive, length_t Columns, length_t Rows, typename Scalar, qualifier Qualifier>
constexpr auto serialize(Archive& archive, mat<Columns, Rows, Scalar, Qualifier>& value)
{
    return io::glm_serialization::serialize_components(archive, value, std::make_integer_sequence<length_t, Columns> {});
}

template <typename Archive, length_t Columns, length_t Rows, typename Scalar, qualifier Qualifier>
constexpr auto serialize(Archive& archive, const mat<Columns, Rows, Scalar, Qualifier>& value)
{
    return io::glm_serialization::serialize_components(archive, value, std::make_integer_sequence<length_t, Columns> {});
}

} // namespace glm

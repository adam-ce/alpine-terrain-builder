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

#include <cstdint>
#include <string>
#include <type_traits>

#include <glm/glm.hpp>

namespace raster_store::pixel {

enum class Mapping {
    Linear,
    SRGBA,
};

namespace detail {

    template <typename PixelType>
    struct Format {
        static_assert(std::is_trivially_copyable_v<PixelType>);
        static_assert(
            (std::is_integral_v<PixelType> && !std::is_same_v<PixelType, bool>) || std::is_same_v<PixelType, float> || std::is_same_v<PixelType, double>);

        static std::string identifier()
        {
            if constexpr (std::is_floating_point_v<PixelType>) {
                return "float" + std::to_string(sizeof(PixelType) * 8);
            } else {
                return std::string(std::is_signed_v<PixelType> ? "int" : "uint") + std::to_string(sizeof(PixelType) * 8);
            }
        }
    };

    template <glm::length_t Length, typename Scalar, glm::qualifier Qualifier>
    struct Format<glm::vec<Length, Scalar, Qualifier>> {
        using PixelType = glm::vec<Length, Scalar, Qualifier>;
        static_assert(std::is_trivially_copyable_v<PixelType>);
        static_assert(sizeof(PixelType) == Length * sizeof(Scalar), "AMORT requires unpadded GLM vectors");

        static std::string identifier() { return "vec" + std::to_string(Length) + "<" + Format<Scalar>::identifier() + ">"; }
    };

} // namespace detail

template <typename PixelType>
inline constexpr Mapping default_mapping = Mapping::Linear;

template <glm::length_t Length, glm::qualifier Qualifier>
inline constexpr Mapping default_mapping<glm::vec<Length, std::uint8_t, Qualifier>> = Length == 3 || Length == 4 ? Mapping::SRGBA : Mapping::Linear;

template <typename PixelType>
std::string identifier()
{
    return detail::Format<PixelType>::identifier();
}

} // namespace raster_store::pixel

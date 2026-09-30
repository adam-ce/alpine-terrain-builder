#ifndef CTBTYPES_HPP
#define CTBTYPES_HPP

/*******************************************************************************
 * Copyright 2014 GeoData <geodata@soton.ac.uk>
 * Copyright 2021 Adam Celarek-Litofcenko
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *******************************************************************************/

/**
 * @file types.hpp
 * @brief This declares basic types used by libctb
 */

#include <cstdint> // uint16_t
#include <glm/glm.hpp>

/// All terrain related data types reside in this namespace
namespace ctb {

// Simple types
using i_pixel = unsigned int; ///< A pixel value
using i_tile = unsigned int; ///< A tile coordinate
using i_zoom = unsigned int; ///< A zoom level
using i_terrain_height = uint16_t; ///< A terrain tile height

// Complex types
using PixelPoint = glm::tvec2<i_pixel>; ///< The location of a pixel
using CRSPoint = glm::tvec2<double>; ///< A Coordinate Reference System coordinate
using TilePoint = glm::tvec2<i_tile>; ///< The location of a tile

}

#endif /* CTBTYPES_HPP */

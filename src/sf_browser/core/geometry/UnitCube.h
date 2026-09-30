/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Adrian Gawor
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
#include <glm/glm.hpp>
#include <vector>

class UnitCube {
public:
	static constexpr std::vector<glm::vec3> vertices() {
		return {
            glm::vec3(-0.5, -0.5, -0.5),    //000  //#0
            glm::vec3(-0.5,  0.5, -0.5),    //010  //#1
            glm::vec3( 0.5,  0.5, -0.5),    //110  //#2
            glm::vec3( 0.5, -0.5, -0.5),    //100  //#3

            glm::vec3(-0.5, -0.5,  0.5),    //001   //#4
            glm::vec3(-0.5,  0.5,  0.5),    //011   //#5
            glm::vec3( 0.5,  0.5,  0.5),    //111   //#6
            glm::vec3( 0.5, -0.5,  0.5),    //101   //#7
        };
	}

    static constexpr std::vector<unsigned int> line_indices() {
        return {
            //Bottom Loop
            0, 1, 1, 2, 2, 3, 3, 0,
            //Top Loop
            4, 5, 5, 6, 6, 7, 7, 4,
            //Connecting Top and Bottom
            0, 4,
            1, 5,
            2, 6,
            3, 7
        };
    }
};
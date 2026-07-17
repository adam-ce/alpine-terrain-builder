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
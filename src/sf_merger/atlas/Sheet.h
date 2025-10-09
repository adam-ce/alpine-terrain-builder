#pragma once

#include <vector>

#include "mesh/SimpleMesh.h"

namespace atlas {

struct Sheet {
    std::vector<std::vector<SimpleMesh::Uv>> uvs;
    SimpleMesh::Texture texture;
};

}

#pragma once

#include <string>
#include <unordered_map>

namespace mesh {
namespace io {
    
struct LoadOptions {
};

struct SaveOptions {
    std::string texture_format = ".jpeg";
    std::string name = "Terrain";
    std::unordered_map<std::string, std::string> metadata = {};
};

} // namespace io
} // namespace mesh

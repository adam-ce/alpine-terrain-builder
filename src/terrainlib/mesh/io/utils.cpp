#include "mesh/io/utils.h"

namespace mesh::io::utils {

std::filesystem::path create_parent_directories(const std::filesystem::path &path) {
    const std::filesystem::path parent_path = std::filesystem::absolute(path).parent_path();
    std::filesystem::create_directories(parent_path);
    return parent_path;
}

} // namespace mesh::io::utils

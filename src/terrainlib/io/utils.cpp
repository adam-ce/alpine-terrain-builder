#include "io/utils.h"

namespace io::utils {

std::error_code create_parent_directories(const std::filesystem::path &path) {
    std::error_code error;
    const std::filesystem::path parent_path = std::filesystem::absolute(path, error).parent_path();
    if (error) {
        return error;
    }
    std::filesystem::create_directories(parent_path, error);
    return error;
}

} // namespace io::utils

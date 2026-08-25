#include <fstream>
#include <system_error>

#include "io/bytes.h"
#include "io/utils.h"
#include "log.h"

namespace io {

std::expected<void, Error> write_bytes_to_path(const std::span<const uint8_t> bytes, const std::filesystem::path &path, bool make_dirs) {
    LOG_TRACE("Writing bytes to path {}", path);

    if (make_dirs) {
        const std::error_code error = utils::create_parent_directories(path);
        if (error) {
            LOG_ERROR("Failed to create parent directories for {}: {}", path, error.message());
            return std::unexpected(Error::CreateDirectories);
        }
    }
    
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        LOG_DEBUG("Failed to open file for writing {}", path);
        return std::unexpected(Error::OpenFile);
    }

    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file.good()) {
        LOG_ERROR("Failed to write bytes to file {}", path);
        return std::unexpected(Error::WriteBytes);
    }

    return {};
}

std::expected<std::vector<uint8_t>, Error> read_bytes_from_path(const std::filesystem::path& path) {
    LOG_TRACE("Reading bytes from path {}", path);

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_DEBUG("Failed to open file for reading {}", path);
        return std::unexpected(Error::OpenFile);
    }

    const std::streamsize size = file.tellg();
    if (size < 0) {
        LOG_ERROR("Failed to determine size for file {}", path);
        return std::unexpected(Error::DetermineSize);
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char *>(buffer.data()), size);

    if (!file.good()) {
        LOG_ERROR("Failed to read bytes from file {}", path);
        return std::unexpected(Error::ReadBytes);
    }

    return buffer;
}

}

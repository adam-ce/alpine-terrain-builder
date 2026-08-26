#include <fstream>
#include <system_error>

#include "io/bytes.h"
#include "io/utils.h"
#include "log.h"

namespace io {

std::expected<void, ::Error> write_bytes_to_path(const std::span<const uint8_t> bytes, const std::filesystem::path& path, bool make_dirs)
{
    LOG_TRACE("Writing bytes to path {}", path);

    if (make_dirs) {
        const std::error_code error = utils::create_parent_directories(path);
        if (error) {
            return std::unexpected(::Error::make(::Error::Code::Io, "create parent directories for", path, error));
        }
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected(::Error::make(::Error::Code::Io, "open file for writing", path));
    }

    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file.good()) {
        return std::unexpected(::Error::make(::Error::Code::Io, "write bytes to", path));
    }

    return {};
}

std::expected<std::vector<uint8_t>, ::Error> read_bytes_from_path(const std::filesystem::path& path)
{
    LOG_TRACE("Reading bytes from path {}", path);

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::error_code existence_error;
        const bool exists = std::filesystem::exists(path, existence_error);
        if (!existence_error && !exists) {
            return std::unexpected(::Error::make(::Error::Code::NotFound, "open file for reading", path));
        }
        return std::unexpected(existence_error
                ? ::Error::make(::Error::Code::Io, "check existence of", path, existence_error)
                : ::Error::make(::Error::Code::Io, "open file for reading", path));
    }

    const std::streamsize size = file.tellg();
    if (size < 0) {
        return std::unexpected(::Error::make(::Error::Code::Io, "determine size of", path));
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char *>(buffer.data()), size);

    if (!file.good()) {
        return std::unexpected(::Error::make(::Error::Code::Io, "read bytes from", path));
    }

    return buffer;
}

}

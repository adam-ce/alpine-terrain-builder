#include "io/utils.h"

#include <cerrno>
#include <cstdio>

#ifdef __linux__
#include <fcntl.h>
#endif

namespace io::utils {

Expected<void> create_parent_directories(const std::filesystem::path& path)
{
    std::error_code error;
    const std::filesystem::path parent_path = std::filesystem::absolute(path, error).parent_path();
    if (error) {
        return Error::fail(Error::Code::Io, "resolve parent directory for", path, error);
    }
    std::filesystem::create_directories(parent_path, error);
    if (error) {
        return Error::fail(error == std::errc::file_exists ? Error::Code::AlreadyExists : Error::Code::Io,
            "create parent directories for", path, error);
    }
    return {};
}

Expected<void> rename_without_replacement(const std::filesystem::path& source, const std::filesystem::path& destination)
{
#ifdef __linux__
    if (::renameat2(AT_FDCWD, source.c_str(), AT_FDCWD, destination.c_str(), RENAME_NOREPLACE) == 0) {
        return {};
    }
    const std::error_code error(errno, std::generic_category());
    if (error == std::errc::no_such_file_or_directory) {
        return Error::fail(Error::Code::NotFound, "rename without replacing destination", source, destination, error);
    }
    return Error::fail(error == std::errc::file_exists ? Error::Code::AlreadyExists : Error::Code::Io,
        "rename without replacing destination", source, destination, error);
#else
    return Error::fail(Error::Code::Unsupported, "rename without replacement is not implemented on this platform",
        source, destination, std::make_error_code(std::errc::operation_not_supported));
#endif
}

} // namespace io::utils

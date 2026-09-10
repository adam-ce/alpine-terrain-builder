#include "inputs.h"

#include <cerrno>
#include <sys/stat.h>

namespace rf_builder::inputs {

Expected<std::string> identifier(const std::string& input)
{
    if (input.empty()) {
        return Error::fail(Error::Code::InvalidInput, "empty RF input identifier");
    }
    if (input.starts_with("http://") || input.starts_with("https://") || input.starts_with("/vsi")) {
        return input;
    }
    std::error_code error;
    const auto path = std::filesystem::absolute(input, error);
    if (error) {
        return Error::fail(Error::Code::Io, "resolve RF input identifier", input, error);
    }
    return path.lexically_normal().string();
}

std::string gdal_identifier(const std::string& input)
{
    return input.starts_with("http://") || input.starts_with("https://") ? "/vsicurl/" + input : input;
}

Expected<void> validate_cache(const std::filesystem::path& path, const Record& record)
{
    auto saved = io::envelope::read_from_path<Schema>(path / file_name);
    if (!saved) {
        return Error::propagate(std::move(saved), "requested RF cache has missing or corrupt inputs.tmp");
    }
    auto normalized = path.lexically_normal();
    if (!normalized.has_filename()) { normalized = normalized.parent_path(); }
    if (normalized.extension() != ".part") {
        return Error::fail(Error::Code::InvalidInput, "RF cache must be an incomplete .part snapshot", path);
    }
    if (*saved != record) {
        return Error::fail(Error::Code::InvalidInput, "requested RF cache inputs.tmp does not match the build inputs or processing settings", path);
    }
    return {};
}

Expected<void> check_link_filesystem(const std::filesystem::path& cache, const std::filesystem::path& output)
{
    struct stat cache_status {};
    if (::stat(cache.c_str(), &cache_status) != 0) {
        return Error::fail(Error::Code::Io, "inspect RF cache filesystem", cache, std::error_code(errno, std::generic_category()));
    }
    auto parent = std::filesystem::absolute(output).parent_path();
    struct stat output_status {};
    while (::stat(parent.c_str(), &output_status) != 0) {
        if (errno != ENOENT || parent == parent.parent_path()) {
            return Error::fail(Error::Code::Io, "inspect RF output filesystem", parent, std::error_code(errno, std::generic_category()));
        }
        parent = parent.parent_path();
    }
    if (cache_status.st_dev != output_status.st_dev) {
        return Error::fail(Error::Code::Unsupported, "RF cache and output must be on the same filesystem for hard-link reuse");
    }
    return {};
}

} // namespace rf_builder::inputs

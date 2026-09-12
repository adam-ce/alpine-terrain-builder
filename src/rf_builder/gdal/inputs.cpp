#include "inputs.h"

namespace rf_builder::gdal::inputs {

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

} // namespace rf_builder::gdal::inputs

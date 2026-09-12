#include "inputs.h"
namespace rf_builder::tiles::inputs {
Expected<void> validate_cache(const std::filesystem::path& path, const Record& record)
{
    auto saved = io::envelope::read_from_path<Schema>(path / "inputs.tmp");
    if (!saved) {
        return Error::propagate(std::move(saved), "requested online RF cache has missing or corrupt inputs.tmp");
    }
    auto normalized = path.lexically_normal();
    if (!normalized.has_filename()) {
        normalized = normalized.parent_path();
    }
    if (normalized.extension() != ".part") {
        return Error::fail(Error::Code::InvalidInput, "online RF cache must be an incomplete .part snapshot");
    }
    if (*saved != record) {
        return Error::fail(Error::Code::InvalidInput, "requested online RF cache inputs.tmp does not match provider, mask, attribution or processing settings");
    }
    return {};
}
} // namespace rf_builder::tiles::inputs

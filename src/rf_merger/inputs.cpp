#include "inputs.h"

#include "io/bytes.h"
#include "raster_store/io/manifest.h"
#include <system_error>

namespace rf_merger::inputs {
namespace {
    Expected<std::uint64_t> hash_file(const std::filesystem::path& path)
    {
        auto bytes = io::read_bytes_from_path(path);
        if (!bytes) {
            return Error::propagate(std::move(bytes), "fingerprint RF input");
        }
        // 64-bit FNV-1a identifies files; it is not a security boundary.
        std::uint64_t hash = 0xcbf29ce484222325ULL;
        for (const auto byte : *bytes) {
            hash = (hash ^ byte) * 0x100000001b3ULL;
        }
        return hash;
    }
} // namespace

Expected<Fingerprint> fingerprint(const std::filesystem::path& snapshot)
{
    std::error_code error;
    const auto canonical = std::filesystem::canonical(snapshot, error);
    if (error) {
        return Error::fail(Error::Code::Io, "resolve RF input path", snapshot, error);
    }
    auto metadata = hash_file(canonical / raster_store::io::manifest::metadata_file_name);
    if (!metadata) {
        return Error::propagate(std::move(metadata));
    }
    auto index = hash_file(canonical / raster_store::io::manifest::index_file_name);
    if (!index) {
        return Error::propagate(std::move(index));
    }
    return Fingerprint { canonical.string(), *metadata, *index };
}

Expected<void> validate_cache(const std::filesystem::path& path, const Record& record)
{
    auto normalized = path.lexically_normal();
    if (!normalized.has_filename()) {
        normalized = normalized.parent_path();
    }
    if (normalized.extension() != ".part") {
        return Error::fail(Error::Code::InvalidInput, "RF merger cache must be an incomplete .part snapshot", path);
    }
    auto saved = io::envelope::read_from_path<Schema>(path / file_name);
    if (!saved) {
        return Error::propagate(std::move(saved), "requested RF merger cache has missing or corrupt inputs.tmp");
    }
    if (*saved != record) {
        return Error::fail(Error::Code::InvalidInput, "requested RF merger cache inputs.tmp does not match the merge inputs or processing settings", path);
    }
    return {};
}

} // namespace rf_merger::inputs

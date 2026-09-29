#include "priorities.h"

#include "io/bytes.h"
#include "raster_store/attribution.h"
#include <algorithm>
#include <cpl_json.h>
#include <string>

namespace rf_merger::priorities {

Expected<std::vector<std::uint16_t>> parse(std::string_view json)
{
    CPLJSONDocument document;
    if (!document.LoadMemory(std::string(json)) || document.GetRoot().GetType() != CPLJSONObject::Type::Array) {
        return Error::fail(Error::Code::InvalidInput, "priority table must be a JSON array of attribution indices");
    }
    std::vector<std::uint16_t> result;
    for (const auto& entry : document.GetRoot().ToArray()) {
        if (entry.GetType() != CPLJSONObject::Type::Integer && entry.GetType() != CPLJSONObject::Type::Long) {
            return Error::fail(Error::Code::InvalidInput, "priority attribution indices must be integers");
        }
        const auto value = entry.ToLong();
        if (value <= 0 || value >= raster_store::attribution::index_limit) {
            return Error::fail(Error::Code::InvalidInput, "priority attribution indices must be in 1..65534");
        }
        if (std::ranges::find(result, std::uint16_t(value)) != result.end()) {
            return Error::fail(Error::Code::InvalidInput, "priority table lists attribution " + std::to_string(value) + " more than once");
        }
        result.push_back(std::uint16_t(value));
    }
    return result;
}

Expected<std::vector<std::uint16_t>> read(const std::filesystem::path& path)
{
    auto bytes = io::read_bytes_from_path(path);
    if (!bytes) {
        return Error::propagate(std::move(bytes), "read priority table");
    }
    auto parsed = parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
    if (!parsed) {
        return Error::propagate(std::move(parsed), "parse priority table \"" + path.string() + "\"");
    }
    return parsed;
}

Ranks ranks(const std::vector<std::uint16_t>& priorities)
{
    Ranks result(raster_store::attribution::index_limit + 1, 1);
    result[0] = 0;
    for (std::size_t i = 0; i < priorities.size(); ++i) {
        result[priorities[i]] = std::uint32_t(2 + priorities.size() - 1 - i);
    }
    return result;
}

} // namespace rf_merger::priorities

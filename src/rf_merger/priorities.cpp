#include "priorities.h"

#include "io/bytes.h"
#include "raster_store/attribution.h"
#include <algorithm>
#include <string>

namespace rf_merger::priorities {

Expected<std::vector<std::uint16_t>> parse(std::string_view json)
{
    std::size_t at = 0;
    const auto whitespace = [&] {
        while (at < json.size() && (json[at] == ' ' || json[at] == '\t' || json[at] == '\r' || json[at] == '\n')) {
            ++at;
        }
    };
    const auto take = [&](char value) {
        whitespace();
        if (at == json.size() || json[at] != value) {
            return false;
        }
        ++at;
        return true;
    };
    const auto invalid = [] { return Error::fail(Error::Code::InvalidInput, "priority table must be a JSON array of attribution indices"); };
    std::vector<std::uint16_t> result;
    if (!take('[')) {
        return invalid();
    }
    if (!take(']')) {
        do {
            whitespace();
            const auto first = at;
            std::uint64_t value = 0;
            while (at < json.size() && json[at] >= '0' && json[at] <= '9' && value < raster_store::attribution::index_limit) {
                value = value * 10 + std::uint64_t(json[at++] - '0');
            }
            if (at == first || (json[first] == '0' && at - first > 1)) {
                return invalid();
            }
            if (at < json.size() && json[at] >= '0' && json[at] <= '9') {
                value = raster_store::attribution::index_limit;
            }
            if (value == 0 || value >= raster_store::attribution::index_limit) {
                return Error::fail(Error::Code::InvalidInput, "priority attribution indices must be in 1..65534");
            }
            if (std::ranges::find(result, std::uint16_t(value)) != result.end()) {
                return Error::fail(Error::Code::InvalidInput, "priority table lists attribution " + std::to_string(value) + " more than once");
            }
            result.push_back(std::uint16_t(value));
        } while (take(','));
        if (!take(']')) {
            return invalid();
        }
    }
    whitespace();
    if (at != json.size()) {
        return invalid();
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

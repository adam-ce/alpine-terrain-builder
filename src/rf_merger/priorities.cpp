/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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

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

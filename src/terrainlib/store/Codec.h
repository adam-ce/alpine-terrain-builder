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

#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include <expected>

#include "Error.h"

namespace store {

template <typename NodeData>
class Codec {
public:
    virtual ~Codec() = default;

    virtual std::vector<std::filesystem::path> paths(const std::filesystem::path& node_path) const = 0;

    virtual Expected<NodeData> read(const std::filesystem::path&) const { return Error::fail(Error::Code::Unsupported, "codec does not support reading"); }

    virtual Expected<void> write(const std::filesystem::path&, const NodeData&) const
    {
        return Error::fail(Error::Code::Unsupported, "codec does not support writing");
    }

protected:
    static std::filesystem::path add_extension(std::filesystem::path path, const std::string_view extension)
    {
        path += extension;
        return path;
    }
};

} // namespace store

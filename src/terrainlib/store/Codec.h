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

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <expected>

#include "Error.h"
#include "io/byte.h"

namespace store {

// Opaque identity of a whole node's stored content, defined by the codec.
using NodeDataHash = io::byte::vector;
using NodeDataHashView = io::byte::span;

inline constexpr std::size_t min_hash_size = 4;

// Identifies a part of a node for partial reads: NodeData::Part if it exists, std::monostate otherwise.
template <typename NodeData>
struct node_part : std::type_identity<std::monostate> { };

template <typename NodeData>
requires requires { typename NodeData::Part; }
struct node_part<NodeData> : std::type_identity<typename NodeData::Part> { };

template <typename NodeData>
using NodePart = typename node_part<NodeData>::type;

// Codec methods must support concurrent calls on the same instance.
// write creates the node's files exclusively and fails with AlreadyExists if
// one of them exists. It never truncates or replaces a file, which may be a
// hard link shared with another snapshot. Reading a node requires its write to
// have completed; exclusive creation alone does not make an unfinished file
// readable.
template <typename NodeData>
class Codec {
public:
    virtual ~Codec() = default;

    virtual std::vector<std::filesystem::path> paths(const std::filesystem::path& node_path) const = 0;

    // A given expected hash must equal the identity of the files read. Storage passes one only
    // to codecs that support hashing.
    virtual Expected<NodeData> read(const std::filesystem::path&, std::optional<NodeDataHashView> /* expected_hash */ = std::nullopt) const
    {
        return Error::fail(Error::Code::Unsupported, "codec does not support reading");
    }

    // Reads only the given part of the node. A given expected hash must equal the identity of the
    // whole node's files, as for read.
    virtual Expected<NodeData> read_part(
        const std::filesystem::path&, const NodePart<NodeData>& /* part */, std::optional<NodeDataHashView> /* expected_hash */ = std::nullopt) const
    {
        return Error::fail(Error::Code::Unsupported, "codec does not support partial reads");
    }

    // Returns the identity of the written node, or an empty hash if the codec does not support hashing.
    virtual Expected<NodeDataHash> write(const std::filesystem::path&, const NodeData&) const
    {
        return Error::fail(Error::Code::Unsupported, "codec does not support writing");
    }

    // Whether writes return whole-node identities of at least min_hash_size bytes and reads compare expected ones.
    virtual bool supports_hashing() const { return false; }

protected:
    static std::filesystem::path add_extension(std::filesystem::path path, const std::string_view extension)
    {
        path += extension;
        return path;
    }
};

} // namespace store

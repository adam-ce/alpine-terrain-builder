#pragma once

#include <string>
#include <variant>

#include "store/IndexFormat.h"
#include "store/OpenError.h"

namespace store {

inline std::string describe_error(const CodecError& error) { return error.message; }

inline std::string describe_error(const IndexFormatError& error) { return error.path.empty() ? error.message : error.path.string() + ": " + error.message; }

inline std::string describe_error(const FilesystemError& error) { return error.operation + " " + error.path.string() + ": " + error.error.message(); }

inline std::string describe_error(const UnknownLayout& error) { return "unknown layout: " + error.id; }

inline std::string describe_error(const AlreadyExists& error) { return "path already exists: " + error.path.string(); }

template <typename Key>
std::string describe_error(const InvalidKey<Key>&)
{
    return "invalid hierarchy key";
}

template <typename Key>
std::string describe_error(const MissingSource<Key>&)
{
    return "source node is missing";
}

template <typename... Errors>
std::string describe_error(const std::variant<Errors...>& error)
{
    return std::visit([](const auto& value) { return describe_error(value); }, error);
}

} // namespace store

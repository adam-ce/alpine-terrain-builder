#pragma once

#include <string>
#include <variant>

#include "store/IndexFormat.h"
#include "store/StorageError.h"

namespace store {

struct UnknownLayout {
    std::string id;
    bool operator==(const UnknownLayout&) const = default;
};

template <typename Key>
using OpenError = std::variant<IndexFormatError, FilesystemError, UnknownLayout, CodecError, InvalidKey<Key>>;

} // namespace store

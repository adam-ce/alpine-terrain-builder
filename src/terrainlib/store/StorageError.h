#pragma once

#include <filesystem>
#include <string>
#include <system_error>
#include <variant>

#include "store/CodecError.h"
#include "store/InvalidKey.h"

namespace store {

struct AlreadyExists {
    std::filesystem::path path;
    bool operator==(const AlreadyExists&) const = default;
};

struct FilesystemError {
    std::filesystem::path path;
    std::string operation;
    std::error_code error;
    bool operator==(const FilesystemError&) const = default;
};

template <typename Key>
struct MissingSource {
    Key key;
    bool operator==(const MissingSource&) const = default;
};

template <typename Key>
using LoadError = std::variant<InvalidKey<Key>, CodecError>;

template <typename Key>
using SaveError = std::variant<InvalidKey<Key>, CodecError, AlreadyExists, FilesystemError>;

template <typename Key>
using FileOperationError = std::variant<InvalidKey<Key>, FilesystemError>;

template <typename Key>
using CopyError = std::variant<InvalidKey<Key>, MissingSource<Key>, AlreadyExists, FilesystemError, CodecError>;

} // namespace store

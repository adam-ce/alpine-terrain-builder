#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace store {

template <typename Key>
struct PathMapping {
    std::string_view id;
    std::filesystem::path (*key_to_node_path)(const Key&);
    std::optional<Key> (*node_path_to_key)(const std::filesystem::path&);

    bool operator==(const PathMapping&) const = default;
};

} // namespace store

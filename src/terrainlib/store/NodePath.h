#pragma once

#include <filesystem>
#include <utility>

namespace store {

class NodePath {
public:
    NodePath() = default;
    explicit NodePath(std::filesystem::path path) : _path(std::move(path)) {}

    const std::filesystem::path &path() const {
        return _path;
    }

    bool operator==(const NodePath &) const = default;

private:
    std::filesystem::path _path;
};

} // namespace store

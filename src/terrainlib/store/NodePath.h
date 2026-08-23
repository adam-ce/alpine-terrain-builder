#pragma once

#include <filesystem>
#include <utility>

namespace store {

class NodePath {
public:
    NodePath() = default;
    explicit NodePath(std::filesystem::path path)
        : m_path(std::move(path))
    {
    }

    const std::filesystem::path& path() const { return m_path; }

    bool operator==(const NodePath&) const = default;

private:
    std::filesystem::path m_path;
};

} // namespace store

#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "id.h"
#include "mesh/io.h"
#include "mesh/SimpleMesh.h"

namespace octree {
using Node = SimpleMesh;

class Storage {
    public:
        Storage(const std::filesystem::path &base_path) : base_path(base_path) {}

        std::optional<Node> load_node(const Id &id) const {
            const auto node_path = this->get_node_path(id);
            const auto result = mesh::io::load_from_path(node_path);
            if (result.has_value()) {
                return result.value();
            } else {
                return std::nullopt;
            }
        }

        bool save_node(const Id &id, const Node &node) const {
            const auto node_path = this->get_node_path(id);
            const auto result = mesh::io::save_to_path(node, node_path);
            return result.has_value();
        }

        bool has_node(const Id &id) const {
            return std::filesystem::exists(this->get_node_path(id));
        }

    private:
        std::filesystem::path get_node_path(const Id &id, std::string_view ext = "glb") const {
            if (!ext.empty() && ext.front() == '.') {
                ext = ext.substr(1); // Remove leading dot
            }

            return base_path
                / std::to_string(id.level())
                / std::to_string(id.x())
                / std::to_string(id.y())
                / fmt::format("{}.{}", id.z(), ext);
        }

    private:
        const std::filesystem::path base_path;
};
}

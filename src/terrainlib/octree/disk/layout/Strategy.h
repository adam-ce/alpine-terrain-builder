#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include "octree/Id.h"

namespace octree::disk::layout {

class Strategy {
public:
    virtual ~Strategy() = default;

    virtual std::filesystem::path get_relative_node_path(Id id, std::string_view extension_with_dot) const = 0;
    virtual std::optional<Id> get_id_from_relative_node_path(const std::filesystem::path &relative_path) const = 0;
};

}

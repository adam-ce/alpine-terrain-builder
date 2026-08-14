#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "octree/store_layout/Flat.h"
#include "octree/store_layout/LevelAndCoordinateDirectories.h"
#include "store/PathMapping.h"

namespace octree::store_layout {

inline store::PathMapping<Id> flat() {
    return {"flat", flat_key_to_node_path, flat_node_path_to_key};
}

inline store::PathMapping<Id> level_and_coordinate_directories() {
    return {
        "level_and_coordinate_directories",
        level_and_coordinate_key_to_node_path,
        level_and_coordinate_node_path_to_key,
    };
}

inline std::array<store::PathMapping<Id>, 2> all() {
    return {flat(), level_and_coordinate_directories()};
}

inline std::optional<store::PathMapping<Id>> from_id(const std::string_view id) {
    for (const auto mapping : all()) {
        if (mapping.id == id) {
            return mapping;
        }
    }
    return std::nullopt;
}

} // namespace octree::store_layout

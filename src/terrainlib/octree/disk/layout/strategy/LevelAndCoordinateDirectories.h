#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include <fmt/core.h>

#include "octree/Id.h"
#include "octree/disk/layout/Strategy.h"
#include "string_utils.h"

namespace octree::disk::layout::strategy {

class LevelAndCoordinateDirectories : public Strategy {
public:
    std::filesystem::path get_relative_node_path(Id id, std::string_view extension_with_dot) const override {
        const Id::Coords coords = id.coords();
        return fmt::format("{}/{}/{}/{}{}", id.level(), coords.x, coords.y, coords.z, extension_with_dot);
    }
    std::optional<Id> get_id_from_relative_node_path(const std::filesystem::path &relative_path) const override {
        DEBUG_ASSERT(relative_path.is_relative());

        auto it = relative_path.begin();
        if (std::distance(it, relative_path.end()) < 4) {
            return std::nullopt;
        }

        const auto level_opt = from_chars<Id::Level>(it->native());
        if (!level_opt) {
            return std::nullopt;
        }
        const Id::Level level = *level_opt;
        ++it;

        const auto x_opt = from_chars<Id::Coord>(it->native());
        if (!x_opt) {
            return std::nullopt;
        }
        const Id::Coord x = *x_opt;
        ++it;

        const auto y_opt = from_chars<Id::Coord>(it->native());
        if (!y_opt) {
            return std::nullopt;
        }
        const Id::Coord y = *y_opt;
        ++it;

        const auto z_opt = from_chars<Id::Coord>(it->stem().native());
        if (!z_opt) {
            return std::nullopt;
        }
        const Id::Coord z = *z_opt;

        return Id::try_make(level, {x, y, z});
    }
};

} // namespace octree::disk::layout::strategy

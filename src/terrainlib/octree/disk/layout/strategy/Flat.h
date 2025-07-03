#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include <fmt/core.h>

#include "octree/Id.h"
#include "octree/disk/layout/Strategy.h"
#include "string_utils.h"

namespace octree::disk::layout::strategy {

class Flat : public Strategy {
public:
    std::filesystem::path get_relative_node_path(Id id, std::string_view extension_with_dot) const override {
        return fmt::format("{}-{}{}", id.level(), id.index_on_level(), extension_with_dot);
    }

    std::optional<Id> get_id_from_relative_node_path(const std::filesystem::path &relative_path) const override {
        DEBUG_ASSERT(relative_path.is_relative());

        const std::string _filestem = relative_path.stem().string();
        const std::string_view filestem = _filestem;

        const size_t dash_pos = filestem.find('-');
        if (dash_pos == std::string::npos) {
            return std::nullopt;
        }
        const std::string_view level_str = filestem.substr(0, dash_pos);
        const std::string_view index_str = filestem.substr(dash_pos + 1);

        const auto level_opt = from_chars<Id::Level>(level_str);
        if (!level_opt) {
            return std::nullopt;
        }

        const auto index_opt = from_chars<Id::Index>(index_str);
        if (!index_opt) {
            return std::nullopt;
        }

        const Id::Level level = *level_opt;
        const Id::Index index = *index_opt;
        return Id::try_make(level, index);
    }
};

} // namespace octree::disk::layout::strategy

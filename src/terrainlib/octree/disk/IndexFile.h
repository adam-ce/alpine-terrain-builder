#pragma once

#include <string>

#include <zpp_bits.h>

#include "octree/IndexMap.h"

namespace octree::disk::v1 {

inline constexpr std::string_view index_file_name() {
    return "terrain.index";
}

struct IndexFile {
    using serialize = zpp::bits::members<3>;

    std::string layout_strategy_id;
    std::string preferred_extension;
    IndexMap map;
};
}

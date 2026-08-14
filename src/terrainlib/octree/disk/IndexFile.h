#pragma once

#include <string>

#include <zpp_bits.h>

#include "octree/StoreTraits.h"
#include "store/Index.h"

namespace octree::disk::v1 {

inline constexpr std::string_view index_file_name() {
    return "terrain.index";
}
inline constexpr std::string_view index_extension() {
    return ".index";
}

struct IndexFile {
    using serialize = zpp::bits::members<3>;

    std::string layout_strategy_id;
    std::string preferred_extension;
    store::Index<StoreTraits> map;
};
}

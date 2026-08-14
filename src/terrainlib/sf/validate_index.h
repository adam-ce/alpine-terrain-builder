#pragma once

#include <expected>

#include "octree/StoreTraits.h"
#include "sf/InvalidTopology.h"
#include "store/Index.h"

namespace sf {

inline std::expected<void, InvalidTopology> validate_index(
    const store::Index<octree::StoreTraits> &index) {
    for (const auto &[key, status] : index) {
        if (status == store::NodeStatus::Inner) {
            return std::unexpected(InvalidTopology{key});
        }
    }
    return {};
}

} // namespace sf

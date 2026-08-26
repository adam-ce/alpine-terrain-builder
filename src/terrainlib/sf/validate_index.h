#pragma once

#include <expected>

#include "octree/StoreTraits.h"
#include "Error.h"
#include "store/Index.h"

namespace sf {

inline std::expected<void, ::Error> validate_index(const store::Index<octree::StoreTraits>& index)
{
    for (const auto& [key, status] : index) {
        if (status == store::NodeStatus::Inner) {
            return std::unexpected(::Error::make(
                ::Error::Code::CorruptData, "Structura Fundamentalis topology contains Inner node " + key.to_string()));
        }
    }
    return {};
}

} // namespace sf

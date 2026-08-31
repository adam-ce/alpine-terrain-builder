#pragma once

#include <expected>

#include "Error.h"
#include "octree/StoreTraits.h"
#include "store/Index.h"

namespace sf {

inline Expected<void> validate_index(const store::Index<octree::StoreTraits>& index)
{
    for (const auto& [key, status] : index) {
        if (status == store::NodeStatus::Inner) {
            return Error::fail(Error::Code::CorruptData, "Structura Fundamentalis topology contains Inner node " + key.to_string());
        }
    }
    return {};
}

} // namespace sf

#pragma once

#include <string>
#include <type_traits>
#include <variant>

#include "octree/Id.h"
#include "sf/InvalidTopology.h"
#include "store/OpenError.h"
#include "store/describe_error.h"

namespace sf {

using FinalizeError = std::variant<store::IndexFormatError, InvalidTopology>;

using ProcessingError = std::variant<InvalidTopology,
    store::OpenError<octree::Id>,
    store::FileOperationError<octree::Id>,
    store::SaveError<octree::Id>,
    store::CopyError<octree::Id>,
    store::IndexFormatError>;

inline std::string describe_error(const InvalidTopology& error) { return "Structura Fundamentalis topology contains Inner node " + error.key.to_string(); }

template <typename... Errors>
std::string describe_error(const std::variant<Errors...>& error)
{
    return std::visit(
        [](const auto& value) -> std::string {
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(value)>, InvalidTopology>) {
                return describe_error(value);
            } else {
                return store::describe_error(value);
            }
        },
        error);
}

} // namespace sf

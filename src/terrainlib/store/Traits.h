#pragma once

#include <concepts>
#include <source_location>
#include <string>

#include "Error.h"

namespace store {

template <typename Traits>
concept HierarchyTraits = requires(typename Traits::Key key) {
    typename Traits::Key;
    typename Traits::Hasher;
    { Traits::root() } -> std::same_as<typename Traits::Key>;
    Traits::parent(key);
    Traits::children(key);
    { Traits::is_valid(key) } -> std::same_as<bool>;
    { Traits::key_to_string(key) } -> std::same_as<std::string>;
};

template <HierarchyTraits Traits>
::Error invalid_key_error(const typename Traits::Key& key,
    const std::source_location location = std::source_location::current())
{
    return ::Error::make(::Error::Code::InvalidInput, "invalid hierarchy key " + Traits::key_to_string(key), location);
}

} // namespace store

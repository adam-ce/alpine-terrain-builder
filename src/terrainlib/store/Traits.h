#pragma once

#include <concepts>

namespace store {

template <typename Traits>
concept HierarchyTraits = requires(typename Traits::Key key) {
    typename Traits::Key;
    typename Traits::Hasher;
    { Traits::root() } -> std::same_as<typename Traits::Key>;
    Traits::parent(key);
    Traits::children(key);
    { Traits::is_valid(key) } -> std::same_as<bool>;
};

} // namespace store

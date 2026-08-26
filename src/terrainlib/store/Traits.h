#pragma once

#include <concepts>
#include <string>

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

template <typename Key>
std::string key_to_string(const Key& key)
{
    if constexpr (requires { key.to_string(); }) {
        return key.to_string();
    } else {
        return to_string(key);
    }
}

} // namespace store

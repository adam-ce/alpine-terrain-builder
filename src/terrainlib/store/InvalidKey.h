#pragma once

namespace store {

template <typename Key>
struct InvalidKey {
    Key key;

    bool operator==(const InvalidKey&) const = default;
};

} // namespace store

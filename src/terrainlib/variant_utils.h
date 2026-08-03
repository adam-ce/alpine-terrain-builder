#pragma once

#include <variant>

template <typename... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template <typename Variant, typename... Ts>
decltype(auto) match(Variant &&variant, Ts &&...visitors) {
    return std::visit(overloaded{std::forward<Ts>(visitors)...}, std::forward<Variant>(variant));
}
#pragma once

#include <type_traits>

// Generic helper to determine the "more exact" type, preferring floating point types
template <typename T1, typename T2>
struct MoreExactType {
private:
    static constexpr bool t1_is_fp = std::is_floating_point_v<T1>;
    static constexpr bool t2_is_fp = std::is_floating_point_v<T2>;

public:
    using type = std::conditional_t<
        (t1_is_fp && !t2_is_fp), T1,
        std::conditional_t<(t2_is_fp && !t1_is_fp), T2,
                           std::conditional_t<(sizeof(T1) >= sizeof(T2)), T1, T2>>>;
};

template <typename T1, typename T2>
using MoreExact = typename MoreExactType<T1, T2>::type;
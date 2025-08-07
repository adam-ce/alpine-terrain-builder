#pragma once

#include <glm/common.hpp>

template <glm::length_t n_dims, glm::length_t current_dim = 0>
class NDLoopHelper {
public:
    using Offset = glm::vec<n_dims, int32_t>;

    template <typename Func>
    static void for_each_offset(const uint32_t radius, Func &&func) {
        Offset offset;
        _for_each_offset(static_cast<int32_t>(radius), offset, std::forward<Func>(func));
    }

private:
    template <typename Func>
    static void _for_each_offset(const int32_t radius, Offset &offset, Func &&func) {
        if constexpr (current_dim == n_dims) {
            func(offset);
        } else {
            for (int32_t i = -radius; i <= radius; i++) {
                offset[current_dim] = i;
                NDLoopHelper<n_dims, current_dim + 1>::_for_each_offset(radius, offset, std::forward<Func>(func));
            }
        }
    }
    
    friend class NDLoopHelper<n_dims, current_dim - 1>;
};

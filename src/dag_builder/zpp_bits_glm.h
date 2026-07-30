#pragma once

#include <zpp_bits.h>
#include <glm/glm.hpp>

#include "glm_utils.h"

namespace zpp::bits {

template <typename Archive, glm::length_t L, typename T, glm::qualifier Q>
auto serialize(Archive &archive, const glm::vec<L, T, Q> &v) {
    return archive(as_span(v));
}

template <typename Archive, glm::length_t L, typename T, glm::qualifier Q>
auto serialize(Archive &archive, glm::vec<L, T, Q> &v) {
    return archive(as_span(v));
}

}

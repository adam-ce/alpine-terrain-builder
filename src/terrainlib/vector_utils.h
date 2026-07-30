#pragma once

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <vector>

#include <libassert/assert.hpp>

// Build a vector of `count` elements, the i-th produced by `generator(i)`.
template <typename Generator>
auto generate_vector(const size_t count, Generator &&generator) {
    std::vector<std::invoke_result_t<Generator &, size_t>> result;
    result.reserve(count);
    for (size_t index = 0; index < count; index++) {
        result.push_back(generator(index));
    }
    return result;
}

template <typename Vector>
void erase_by_index(Vector &vec, size_t index) {
    typename Vector::iterator it = vec.begin();
    std::advance(it, index);
    vec.erase(it);
}

template <typename Vector>
void dedup_by_sort(Vector &vec) {
    std::sort(vec.begin(), vec.end());
    vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}

template <typename Vector>
void remove_first(Vector &vec, const typename Vector::value_type &value) {
    auto it = std::find(vec.begin(), vec.end(), value);
    if (it != vec.end()) {
        vec.erase(it);
    }
}

template <typename Vector>
bool contains(const Vector &vec, const typename Vector::value_type &value) {
    return std::find(vec.begin(), vec.end(), value) != vec.end();
}

template <typename Vector>
auto iterator_from_ref(Vector &vec, typename Vector::value_type &ref) {
    // Make sure ref actually belongs to vec
    DEBUG_ASSERT(&ref >= vec.data() && &ref < vec.data() + vec.size());
    return vec.begin() + (&ref - vec.data());
}
template <typename Vector>
auto iterator_from_ref(const Vector &vec, const typename Vector::value_type &ref) {
    // Make sure ref actually belongs to vec
    DEBUG_ASSERT(&ref >= vec.data() && &ref < vec.data() + vec.size());
    return vec.cbegin() + (&ref - vec.data());
}

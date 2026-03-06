#pragma once

#include <vector>
#include <algorithm>

template <typename T>
void erase_by_index(std::vector<T> &vec, size_t index) {
    typename std::vector<T>::iterator it = vec.begin();
    std::advance(it, index);
    vec.erase(it);
}

template <typename T>
void dedup_by_sort(std::vector<T> &vec) {
    std::sort(vec.begin(), vec.end());
    vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}

template <typename T>
void remove_first(std::vector<T> &vec, const T &value) {
    auto it = std::find(vec.begin(), vec.end(), value);
    if (it != vec.end()) {
        vec.erase(it);
    }
}

template <typename T>
auto iterator_from_ref(std::vector<T> &vec, T &ref) {
    // Make sure ref actually belongs to vec
    DEBUG_ASSERT(&ref >= vec.data() && &ref < vec.data() + vec.size());
    return vec.begin() + (&ref - vec.data());
}
template <typename T>
auto iterator_from_ref(const std::vector<T> &vec, const T &ref) {
    // Make sure ref actually belongs to vec
    DEBUG_ASSERT(&ref >= vec.data() && &ref < vec.data() + vec.size());
    return vec.cbegin() + (&ref - vec.data());
}

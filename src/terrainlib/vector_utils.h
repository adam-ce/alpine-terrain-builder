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

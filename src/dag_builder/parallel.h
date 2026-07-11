#pragma once

#include <tbb/parallel_for.h>
#include <tbb/parallel_for_each.h>

template <typename F>
void parallel_for(size_t begin, size_t end, F &&func, bool do_parallel) {
    if (do_parallel) {
        tbb::parallel_for(begin, end, func);
    } else {
        for (size_t i = begin; i < end; i++) {
            func(i);
        }
    }
}

template <typename Range, typename F>
void parallel_foreach(Range &&range, F &&func, bool do_parallel) {
    if (do_parallel) {
        tbb::parallel_for_each(std::begin(range), std::end(range), std::forward<F>(func));
    } else {
        for (auto &&elem : range) {
            func(elem);
        }
    }
}

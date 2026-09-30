/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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

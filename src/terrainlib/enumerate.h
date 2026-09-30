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

#include <cstddef>
#include <optional>
#include <ranges>
#include <utility>

namespace detail {

template <typename TIndex, typename TValue>
struct enumerate_item {
    TIndex index;
    TValue value;
};

template <typename Iter, typename TIndex>
class enumerate_iterator {
private:
    using _reference = decltype(*std::declval<Iter&>());
    using _item = enumerate_item<TIndex, _reference>;

    TIndex _index{};
    Iter _iter{};
    mutable std::optional<_item> _current{};

public:
    enumerate_iterator() = default;

    enumerate_iterator(const TIndex index, Iter iter)
        : _index(index),
          _iter(std::move(iter)) {}

    auto operator*() const -> _item& {
        this->_current.emplace(_item{this->_index, *this->_iter});
        return *this->_current;
    }

    auto operator++() -> enumerate_iterator& {
        ++this->_index;
        ++this->_iter;
        return *this;
    }

    void operator++(int) {
        ++(*this);
    }

    friend auto operator==(const enumerate_iterator& lhs,
                           const enumerate_iterator& rhs) -> bool {
        return lhs._iter == rhs._iter;
    }
};

template <typename Range, typename TIndex>
class enumerate_view {
private:
    Range _range;

public:
    explicit enumerate_view(Range range)
        : _range(std::move(range)) {}

    auto begin() {
        return enumerate_iterator<
            decltype(std::ranges::begin(this->_range)),
            TIndex>{
            TIndex{0},
            std::ranges::begin(this->_range)
        };
    }

    auto end() {
        return enumerate_iterator<
            decltype(std::ranges::end(this->_range)),
            TIndex>{
            TIndex{0},
            std::ranges::end(this->_range)
        };
    }
};

} // namespace detail

template <typename TIndex = std::size_t, std::ranges::viewable_range Range>
auto enumerate(Range&& range) {
    return detail::enumerate_view<std::views::all_t<Range>, TIndex>{
        std::views::all(std::forward<Range>(range))
    };
}

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

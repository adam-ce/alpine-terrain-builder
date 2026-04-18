#pragma once

#include <algorithm>
#include <concepts>
#include <functional>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

template <typename Iterator, typename T>
Iterator find_single(Iterator begin, Iterator end, const T &value) {
    // Find first occurrence
    auto it = std::ranges::find(begin, end, value);
    if (it == end) {
        return end;
    }

    // Check if another occurrence exists
    if (std::ranges::find(std::next(it), end, value) != end) {
        return end;
    }

    return it;
}
template <std::ranges::input_range Range, typename T>
auto find_single(Range &&range, const T &value) {
    return find_single(std::ranges::begin(range), std::ranges::end(range), value);
}

template <std::ranges::input_range Range, typename T>
std::optional<size_t> find_single_index(const Range &range, const T &value) {
    const auto begin = std::ranges::begin(range);
    const auto end = std::ranges::end(range);

    const auto it = find_single(range, value);
    if (it == end) {
        return std::nullopt;
    }

    return std::distance(begin, it);
}

namespace detail {
template <typename T>
concept tuple_like =
    requires {
        typename std::tuple_size<std::remove_cvref_t<T>>::type;
    };

template <typename Fn>
auto invoke_apply(Fn &&op) {
    return [op = std::forward<Fn>(op)](auto &&item) mutable -> decltype(auto) {
        using T = decltype(item);

        if constexpr (tuple_like<T>) {
            return std::apply(op, std::forward<T>(item));
        } else {
            return op(std::forward<T>(item));
        }
    };
}
} // namespace detail

template <std::ranges::input_range Range, typename Op>
auto transform_vector(Range &&range, Op&& op) {
    auto wrapped = detail::invoke_apply(std::forward<Op>(op));

    using InputType = std::ranges::range_value_t<Range>;
    using OutputType = std::invoke_result_t<decltype(wrapped), InputType>;

    std::vector<OutputType> result;

    // Check if the range has a known size to reserve memory
    if constexpr (std::ranges::sized_range<Range>) {
        result.reserve(std::ranges::size(range));
    }

    std::ranges::transform(range, std::back_inserter(result), wrapped);

    return result;
}

template <std::ranges::input_range Range, typename T, typename F>
constexpr T sum(Range &&range, T init, F &&f) {
    for (auto &&v : range) {
        init += std::invoke(f, v);
    }
    return init;
}

template <std::ranges::input_range Range, typename F>
constexpr auto sum(Range &&range, F &&f) {
    using T = std::decay_t<std::invoke_result_t<F &, std::ranges::range_reference_t<Range>>>;
    return sum(std::forward<Range>(range), T{}, std::forward<F>(f));
}

template <std::ranges::input_range Range, typename T = std::ranges::range_value_t<Range>>
    requires std::convertible_to<std::ranges::range_value_t<Range>, T>
constexpr T sum(Range &&range, T init = T{}) {
    return sum(std::forward<Range>(range), init, std::identity{});
}

template <typename T>
constexpr auto range(T begin, T end) {
    return std::views::iota(begin, end);
}
template <typename T>
constexpr auto range(T end) {
    return range(T{0}, end);
}

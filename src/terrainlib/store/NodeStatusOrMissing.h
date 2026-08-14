#pragma once

#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <magic_enum/magic_enum.hpp>

#include "log.h"
#include "store/NodeStatus.h"

namespace store {

class NodeStatusOrMissing {
public:
    using Underlying = NodeStatus::Underlying;
    enum Value : Underlying {
        Leaf = 0,
        Inner = 1,
        Virtual = 2,
        Missing = 3,
    };

    constexpr NodeStatusOrMissing() = default;
    constexpr NodeStatusOrMissing(const Value value) : _value(value) {}
    NodeStatusOrMissing(const NodeStatus status) : _value(from_status(status)) {}
    NodeStatusOrMissing(const std::optional<NodeStatus> status) : _value(from_optional_status(status)) {}

    constexpr operator Value() const {
        return _value;
    }
    constexpr operator std::optional<NodeStatus>() const {
        return to_optional_status();
    }
    explicit operator bool() const = delete;
    constexpr bool operator==(const NodeStatusOrMissing other) const {
        return _value == other._value;
    }
    constexpr bool operator!=(const NodeStatusOrMissing other) const {
        return !(*this == other);
    }
    constexpr bool operator==(const Value other) const {
        return _value == other;
    }
    constexpr bool operator!=(const Value other) const {
        return _value != other;
    }

    std::string to_string() const;

private:
    static constexpr NodeStatusOrMissing from_status(const NodeStatus status) noexcept {
        return static_cast<Value>(static_cast<NodeStatus::Value>(status));
    }
    static constexpr NodeStatusOrMissing from_optional_status(const std::optional<NodeStatus> status) noexcept {
        return status.has_value()
            ? from_status(status.value())
            : NodeStatusOrMissing(NodeStatusOrMissing::Missing);
    }
    constexpr std::optional<NodeStatus> to_optional_status() const noexcept {
        if (_value == Missing) {
            return std::nullopt;
        }
        return static_cast<NodeStatus::Value>(_value);
    }

public:
    Value _value;
};

namespace detail {
template<std::size_t... Indices>
consteval bool verify_node_status_values(std::index_sequence<Indices...>) {
    constexpr auto status_values = magic_enum::enum_values<NodeStatus::Value>();
    constexpr auto optional_values = magic_enum::enum_values<NodeStatusOrMissing::Value>();
    return ((magic_enum::enum_underlying(status_values[Indices])
             == magic_enum::enum_underlying(optional_values[Indices]))
            && ...);
}

consteval bool verify_node_status_enums() {
    static_assert(
        std::is_same_v<
            magic_enum::underlying_type_t<NodeStatus::Value>,
            magic_enum::underlying_type_t<NodeStatusOrMissing::Value>>);
    constexpr auto status_values = magic_enum::enum_values<NodeStatus::Value>();
    constexpr auto optional_values = magic_enum::enum_values<NodeStatusOrMissing::Value>();
    static_assert(optional_values.size() == status_values.size() + 1);
    static_assert(verify_node_status_values(std::make_index_sequence<status_values.size()>()));
    return true;
}

static_assert(verify_node_status_enums());
} // namespace detail

} // namespace store

template<>
struct fmt::formatter<store::NodeStatusOrMissing> {
    template<typename ParseContext>
    constexpr auto parse(ParseContext &context) {
        return context.begin();
    }

    template<typename FormatContext>
    auto format(const store::NodeStatusOrMissing &status, FormatContext &context) const {
        switch (status) {
        case store::NodeStatusOrMissing::Leaf:
            return fmt::format_to(context.out(), "Leaf");
        case store::NodeStatusOrMissing::Inner:
            return fmt::format_to(context.out(), "Inner");
        case store::NodeStatusOrMissing::Virtual:
            return fmt::format_to(context.out(), "Virtual");
        case store::NodeStatusOrMissing::Missing:
            return fmt::format_to(context.out(), "Missing");
        default:
            UNREACHABLE();
        }
    }
};

inline std::string store::NodeStatusOrMissing::to_string() const {
    return fmt::format("{}", *this);
}

inline std::ostream &operator<<(std::ostream &stream, const store::NodeStatusOrMissing &status) {
    fmt::print(stream, "{}", status);
    return stream;
}

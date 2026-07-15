#pragma once

#include <optional>

#include <magic_enum/magic_enum.hpp>

#include "octree/NodeStatus.h"

namespace octree {

class NodeStatusOrMissing {
public:
    using Underlying = NodeStatus::Underlying;
    enum Value : Underlying {
        Leaf = 0,
        Inner = 1,
        Virtual = 2, // not present on disk but has children
        Missing = 3
    };

    constexpr NodeStatusOrMissing() = default;
    constexpr NodeStatusOrMissing(Value value) : _value(value) {}
    NodeStatusOrMissing(NodeStatus opt) : _value(from_status(opt)) {}
    NodeStatusOrMissing(std::optional<NodeStatus> opt) : _value(from_optional_status(opt)) {}

    constexpr operator Value() const {
        return this->_value;
    }
    constexpr operator std::optional<NodeStatus>() const {
        return this->to_optional_status();
    }
    explicit operator bool() const = delete;
    constexpr bool operator==(NodeStatusOrMissing other) const {
        return this->_value == other._value;
    }
    constexpr bool operator!=(NodeStatusOrMissing other) const {
        return !(*this == other);
    }
    constexpr bool operator==(Value other_value) const {
        return this->_value == other_value;
    }
    constexpr bool operator!=(Value other_value) const {
        return this->_value != other_value;
    }

    std::string to_string() const;

private:
    constexpr static NodeStatusOrMissing from_status(NodeStatus status) noexcept {
        return static_cast<Value>(static_cast<NodeStatus::Value>(status));
    }
    constexpr static NodeStatusOrMissing from_optional_status(std::optional<NodeStatus> opt) noexcept {
        if (opt.has_value()) {
            return from_status(opt.value());
        } else {
            return NodeStatusOrMissing::Missing;
        }
    }
    constexpr std::optional<NodeStatus> to_optional_status() const noexcept {
        if (this->_value == NodeStatusOrMissing::Missing) {
            return std::nullopt;
        } else {
            return static_cast<NodeStatus::Value>(this->_value);
        }
    }

public:
    Value _value;
};

}

#include <fmt/format.h>
template <>
struct fmt::formatter<octree::NodeStatusOrMissing> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const octree::NodeStatusOrMissing &status, FormatContext &ctx) const {
        switch (status) {
        case octree::NodeStatusOrMissing::Leaf:
            return fmt::format_to(ctx.out(), "Leaf");
        case octree::NodeStatusOrMissing::Inner:
            return fmt::format_to(ctx.out(), "Inner");
        case octree::NodeStatusOrMissing::Virtual:
            return fmt::format_to(ctx.out(), "Virtual");
        case octree::NodeStatusOrMissing::Missing:
            return fmt::format_to(ctx.out(), "Missing");
        default:
            UNREACHABLE();
        }
    }
};

#include <fmt/ostream.h>
#include <iostream>
inline std::string octree::NodeStatusOrMissing::to_string() const {
    return fmt::format("{}", *this);
}
inline std::ostream &operator<<(std::ostream &os, const octree::NodeStatusOrMissing &status) {
    fmt::print(os, "{}", status);
    return os;
}

namespace octree {
namespace {
template <std::size_t... Is>
consteval bool _verify_enum_values(std::index_sequence<Is...>) {
    constexpr auto ns_values = magic_enum::enum_values<NodeStatus::Value>();
    constexpr auto nsom_values = magic_enum::enum_values<NodeStatusOrMissing::Value>();

    return ((magic_enum::enum_underlying(ns_values[Is]) ==
             magic_enum::enum_underlying(nsom_values[Is])) &&
            ...);
}

consteval bool _verify_enum() {
    static_assert(
        std::is_same_v<magic_enum::underlying_type_t<NodeStatus::Value>,
                       magic_enum::underlying_type_t<NodeStatusOrMissing::Value>>,
        "Mismatched underlying type");

    constexpr auto ns_values = magic_enum::enum_values<NodeStatus::Value>();
    constexpr auto nsom_values = magic_enum::enum_values<NodeStatusOrMissing::Value>();

    static_assert(nsom_values.size() == ns_values.size() + 1, "Mismatched enum counts");

    static_assert(_verify_enum_values(std::make_index_sequence<ns_values.size()>()),
                  "Enum value mismatch");

    return true;
}
static_assert(_verify_enum());
}
}

#pragma once

#include <zpp_bits.h>

#include "log.h"
#include "octree/Id.h"

namespace octree {

class NodeStatus {
public:
    enum Value : uint8_t {
        Leaf = 0,
        Inner = 1,
        Virtual = 2 // not present on disk but has children
    };

    NodeStatus() = default;
    constexpr NodeStatus(Value value) : _value(value) {}

    constexpr operator Value() const {
        return this->_value;
    }
    explicit operator bool() const = delete;
    constexpr bool operator==(NodeStatus other) const {
        return this->_value == other._value;
    }
    constexpr bool operator!=(NodeStatus other) const {
        return !(*this == other);
    }

    std::string to_string() const;

private:
    Value _value;

public:
    using serialize = zpp::bits::members<1>;
    friend zpp::bits::access;
};

}

#include <fmt/format.h>
template <>
struct fmt::formatter<octree::NodeStatus> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const octree::NodeStatus &status, FormatContext &ctx) {
        switch (status) {
        case octree::NodeStatus::Leaf:
            return fmt::format_to(ctx.out(), "Leaf");
        case octree::NodeStatus::Inner:
            return fmt::format_to(ctx.out(), "Inner");
        case octree::NodeStatus::Virtual:
            return fmt::format_to(ctx.out(), "Virtual");
        default:
            UNREACHABLE();
        }
    }
};

#include <fmt/ostream.h>
#include <iostream>
inline std::string octree::NodeStatus::to_string() const {
    return fmt::format("{}", *this);
}
inline std::ostream &operator<<(std::ostream &os, const octree::NodeStatus &status) {
    fmt::print(os, "{}", status);
    return os;
}

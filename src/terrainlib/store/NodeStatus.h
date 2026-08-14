#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <zpp_bits.h>

#include "log.h"

namespace store {

class NodeStatus {
public:
    using Underlying = uint8_t;
    enum Value : Underlying {
        Leaf = 0,
        Inner = 1,
        Virtual = 2,
    };

    constexpr NodeStatus() = default;
    constexpr NodeStatus(const Value value) : _value(value) {}

    constexpr operator Value() const {
        return _value;
    }
    explicit operator bool() const = delete;
    constexpr bool operator==(const NodeStatus other) const {
        return _value == other._value;
    }
    constexpr bool operator!=(const NodeStatus other) const {
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
    Value _value;

public:
    using serialize = zpp::bits::members<1>;
    friend zpp::bits::access;
};

} // namespace store

template<>
struct fmt::formatter<store::NodeStatus> {
    template<typename ParseContext>
    constexpr auto parse(ParseContext &context) {
        return context.begin();
    }

    template<typename FormatContext>
    auto format(const store::NodeStatus &status, FormatContext &context) const {
        switch (status) {
        case store::NodeStatus::Leaf:
            return fmt::format_to(context.out(), "Leaf");
        case store::NodeStatus::Inner:
            return fmt::format_to(context.out(), "Inner");
        case store::NodeStatus::Virtual:
            return fmt::format_to(context.out(), "Virtual");
        default:
            UNREACHABLE();
        }
    }
};

inline std::string store::NodeStatus::to_string() const {
    return fmt::format("{}", *this);
}

inline std::ostream &operator<<(std::ostream &stream, const store::NodeStatus &status) {
    fmt::print(stream, "{}", status);
    return stream;
}

#pragma once

#include <zpp_bits.h>
#include <libassert/assert.hpp>

#include "log.h"

namespace io {

class Error {
public:
    enum Value : uint8_t {
        OpenFile,
        WriteBytes,
        DetermineSize,
        ReadBytes,
        Serialize,
        Deserialize,
        OutOfMemory
    };

    Error() = default;
    constexpr Error(Value value) : _value(value) {}

    constexpr operator Value() const {
        return this->_value;
    }
    explicit operator bool() const = delete;
    constexpr bool operator==(Error other) const {
        return this->_value == other._value;
    }
    constexpr bool operator!=(Error other) const {
        return !(*this == other);
    }

    std::string to_string() const;

private:
    Value _value;

public:
    using serialize = zpp::bits::members<1>;
    friend zpp::bits::access;
};

} // namespace io

#include <fmt/format.h>
template <>
struct fmt::formatter<io::Error> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const io::Error &error, FormatContext &ctx) const {
        const char *name = "Unknown";

        switch (error) {
        case io::Error::Value::OpenFile:
            name = "OpenFile";
            break;
        case io::Error::Value::WriteBytes:
            name = "WriteBytes";
            break;
        case io::Error::Value::DetermineSize:
            name = "DetermineSize";
            break;
        case io::Error::Value::ReadBytes:
            name = "ReadBytes";
            break;
        case io::Error::Value::Serialize:
            name = "Serialize";
            break;
        case io::Error::Value::Deserialize:
            name = "Deserialize";
            break;
        case io::Error::Value::OutOfMemory:
            name = "OutOfMemory";
            break;
        default:
            UNREACHABLE();
        }

        return fmt::format_to(ctx.out(), "{}", name);
    }
};

#include <fmt/ostream.h>
#include <iostream>
inline std::string io::Error::to_string() const {
    return fmt::format("{}", *this);
}
inline std::ostream &operator<<(std::ostream &os, const io::Error &status) {
    fmt::print(os, "{}", status);
    return os;
}

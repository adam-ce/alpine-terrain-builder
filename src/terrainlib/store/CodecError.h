#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace store {

enum class CodecOperation {
    Read,
    Write,
    Resolve,
};

enum class CodecErrorCategory {
    UnsupportedOperation,
    UnsupportedCodec,
    FileNotFound,
    InvalidData,
    Io,
    Domain,
};

struct CodecError {
    CodecOperation operation;
    CodecErrorCategory category;
    std::string message;

    static CodecError unsupported_operation(
        const CodecOperation operation,
        const std::string_view name) {
        return {operation, CodecErrorCategory::UnsupportedOperation, std::string(name)};
    }

    bool operator==(const CodecError &) const = default;
};

} // namespace store

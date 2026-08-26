#pragma once

#include <filesystem>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

class Error {
public:
    enum class Code {
        /// A caller supplied an invalid value, such as a malformed hierarchy key or incompatible dimensions.
        InvalidInput,
        /// A requested file, node, or key does not exist.
        NotFound,
        /// Creating or publishing an object failed because the destination already exists.
        AlreadyExists,
        /// A requested codec, layout, format version, algorithm, or data type is not supported.
        Unsupported,
        /// Persisted or external data violates its format or invariants, such as a checksum mismatch or invalid topology.
        CorruptData,
        /// A filesystem or device operation failed, excluding failures classified more specifically above.
        Io,
        /// An explicit size, storage, or other recoverable resource limit was exceeded.
        ResourceExhausted,
        /// An internal invariant or an otherwise valid operation failed unexpectedly.
        Internal,
    };

    struct Frame {
        std::string message;
        std::source_location location;
        std::optional<Code> original_code;
        std::optional<Code> new_code;
    };

    static Error make(Code code,
        std::string message,
        std::source_location location = std::source_location::current());
    static Error make(Code code,
        std::string_view operation,
        const std::filesystem::path& path,
        std::source_location location = std::source_location::current());
    static Error make(Code code,
        std::string_view operation,
        const std::error_code& cause,
        std::source_location location = std::source_location::current());
    static Error make(Code code,
        std::string_view operation,
        const std::filesystem::path& path,
        const std::error_code& cause,
        std::source_location location = std::source_location::current());
    static Error make(Code code,
        std::string_view operation,
        const std::filesystem::path& source,
        const std::filesystem::path& destination,
        const std::error_code& cause,
        std::source_location location = std::source_location::current());

    [[nodiscard]] Error with_context(
        std::string message,
        std::source_location location = std::source_location::current()) &&;
    [[nodiscard]] Error reclassified(Code code,
        std::string message,
        std::source_location location = std::source_location::current()) &&;

    Code code() const { return m_code; }
    std::span<const Frame> frames() const { return m_frames; }
    std::string to_string() const;

private:
    Error(Code code, Frame frame);

    Code m_code;
    std::vector<Frame> m_frames;
};

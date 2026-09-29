#pragma once

#include <exception>
#include <expected>
#include <filesystem>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace cpptrace {
struct raw_trace;
}

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
        /// An operation was cancelled before publication or completion.
        Cancelled,
        /// An internal invariant or an otherwise valid operation failed unexpectedly.
        Internal,
    };

    struct Frame {
        std::string message;
        std::source_location location;
    };

    // Thrown by tools for errors they cannot recover from; reusable code returns Expected.
    class Exception;

    static Error make(Code code, std::string message, std::source_location location = std::source_location::current());
    static Error make(
        Code code, std::string_view operation, const std::filesystem::path& path, std::source_location location = std::source_location::current());
    static Error make(Code code, std::string_view operation, const std::error_code& cause, std::source_location location = std::source_location::current());
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

    static std::unexpected<Error> fail(Code code, std::string message, std::source_location location = std::source_location::current());
    static std::unexpected<Error> fail(
        Code code, std::string_view operation, const std::filesystem::path& path, std::source_location location = std::source_location::current());
    static std::unexpected<Error> fail(
        Code code, std::string_view operation, const std::error_code& cause, std::source_location location = std::source_location::current());
    static std::unexpected<Error> fail(Code code,
        std::string_view operation,
        const std::filesystem::path& path,
        const std::error_code& cause,
        std::source_location location = std::source_location::current());
    static std::unexpected<Error> fail(Code code,
        std::string_view operation,
        const std::filesystem::path& source,
        const std::filesystem::path& destination,
        const std::error_code& cause,
        std::source_location location = std::source_location::current());

    template <typename T>
    static std::unexpected<Error> propagate(
        std::expected<T, Error>&& result, std::string message = "propagated", std::source_location location = std::source_location::current());
    template <typename T>
    static std::unexpected<Error> propagate(
        std::expected<T, Error>&& result, Code code, std::string message, std::source_location location = std::source_location::current());
    static std::unexpected<Error> propagate(Error&& error, std::string message = "propagated", std::source_location location = std::source_location::current());

    // Throw an Exception for a new error, like fail.
    [[noreturn]] static void raise(Code code, std::string message, std::source_location location = std::source_location::current());
    [[noreturn]] static void raise(
        Code code, std::string_view operation, const std::filesystem::path& path, std::source_location location = std::source_location::current());
    [[noreturn]] static void raise(
        Code code, std::string_view operation, const std::error_code& cause, std::source_location location = std::source_location::current());
    [[noreturn]] static void raise(Code code,
        std::string_view operation,
        const std::filesystem::path& path,
        const std::error_code& cause,
        std::source_location location = std::source_location::current());
    [[noreturn]] static void raise(Code code,
        std::string_view operation,
        const std::filesystem::path& source,
        const std::filesystem::path& destination,
        const std::error_code& cause,
        std::source_location location = std::source_location::current());

    // Returns the value, or throws an Exception for an error the tool cannot
    // recover from. A nonempty message adds a context frame.
    template <typename T>
    static T throwing_unwrap(std::expected<T, Error>&& result, std::string message = {}, std::source_location location = std::source_location::current());

    // Returns the value; a failure is a bug and panics through libassert.
    template <typename T>
    static T asserting_unwrap(std::expected<T, Error>&& result, std::source_location location = std::source_location::current());

    Code code() const { return m_code; }
    std::string to_string() const;
    // The stack trace where the error was made, resolved on demand.
    std::string stacktrace() const;

private:
    Error(Code code, Frame frame);
    [[nodiscard]] Error with_context(std::string message, std::source_location location) &&;
    [[nodiscard]] Error reclassified(Code code, std::string message, std::source_location location) &&;
    [[noreturn]] static void panic(const Error& error, std::source_location location);

    Code m_code;
    std::vector<Frame> m_frames;
    std::shared_ptr<const cpptrace::raw_trace> m_stacktrace;
};

class Error::Exception : public std::exception {
public:
    explicit Exception(Error error);

    const char* what() const noexcept override { return m_what.c_str(); }
    const Error& error() const noexcept { return m_error; }

private:
    Error m_error;
    std::string m_what;
};

template <typename T>
using Expected = std::expected<T, Error>;

template <typename T>
std::unexpected<Error> Error::propagate(std::expected<T, Error>&& result, std::string message, const std::source_location location)
{
    return std::unexpected<Error> {
        std::move(result).error().with_context(std::move(message), location),
    };
}

template <typename T>
std::unexpected<Error> Error::propagate(std::expected<T, Error>&& result, const Code code, std::string message, const std::source_location location)
{
    return std::unexpected<Error> {
        std::move(result).error().reclassified(code, std::move(message), location),
    };
}

template <typename T>
T Error::throwing_unwrap(std::expected<T, Error>&& result, std::string message, const std::source_location location)
{
    if (!result) {
        if (message.empty()) {
            throw Exception(std::move(result).error());
        }
        throw Exception(std::move(result).error().with_context(std::move(message), location));
    }
    if constexpr (!std::is_void_v<T>) {
        return std::move(*result);
    }
}

template <typename T>
T Error::asserting_unwrap(std::expected<T, Error>&& result, const std::source_location location)
{
    if (!result) {
        panic(result.error(), location);
    }
    if constexpr (!std::is_void_v<T>) {
        return std::move(*result);
    }
}

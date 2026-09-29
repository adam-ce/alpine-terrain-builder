#include "Error.h"

#include <cstdlib>
#include <utility>

#include <cpptrace/cpptrace.hpp>
#include <fmt/format.h>
#include <libassert/assert.hpp>

namespace {

std::string_view code_name(const Error::Code code)
{
    switch (code) {
    case Error::Code::InvalidInput:
        return "InvalidInput";
    case Error::Code::NotFound:
        return "NotFound";
    case Error::Code::AlreadyExists:
        return "AlreadyExists";
    case Error::Code::Unsupported:
        return "Unsupported";
    case Error::Code::CorruptData:
        return "CorruptData";
    case Error::Code::Io:
        return "Io";
    case Error::Code::ResourceExhausted:
        return "ResourceExhausted";
    case Error::Code::Cancelled:
        return "Cancelled";
    case Error::Code::Internal:
        return "Internal";
    }
    return "Unknown";
}

std::string describe_system_error(const std::error_code& cause) { return fmt::format("{} ({}:{})", cause.message(), cause.category().name(), cause.value()); }

} // namespace

Error::Error(const Code code, Frame frame)
    : m_code(code)
    // Only the raw addresses are captured here; symbols are resolved when printed.
    , m_stacktrace(std::make_shared<const cpptrace::raw_trace>(cpptrace::generate_raw_trace(1)))
{
    m_frames.push_back(std::move(frame));
}

Error Error::make(const Code code, std::string message, const std::source_location location) { return Error(code, Frame { std::move(message), location }); }

Error Error::make(const Code code, const std::string_view operation, const std::filesystem::path& path, const std::source_location location)
{
    return make(code, fmt::format("{} \"{}\"", operation, path.string()), location);
}

Error Error::make(const Code code, const std::string_view operation, const std::error_code& cause, const std::source_location location)
{
    return make(code, fmt::format("{}: {}", operation, describe_system_error(cause)), location);
}

Error Error::make(
    const Code code, const std::string_view operation, const std::filesystem::path& path, const std::error_code& cause, const std::source_location location)
{
    return make(code, fmt::format("{} \"{}\": {}", operation, path.string(), describe_system_error(cause)), location);
}

Error Error::make(const Code code,
    const std::string_view operation,
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    const std::error_code& cause,
    const std::source_location location)
{
    return make(code, fmt::format("{} \"{}\" -> \"{}\": {}", operation, source.string(), destination.string(), describe_system_error(cause)), location);
}

std::unexpected<Error> Error::fail(const Code code, std::string message, const std::source_location location)
{
    return std::unexpected<Error> { make(code, std::move(message), location) };
}

std::unexpected<Error> Error::fail(const Code code, const std::string_view operation, const std::filesystem::path& path, const std::source_location location)
{
    return std::unexpected<Error> { make(code, operation, path, location) };
}

std::unexpected<Error> Error::fail(const Code code, const std::string_view operation, const std::error_code& cause, const std::source_location location)
{
    return std::unexpected<Error> { make(code, operation, cause, location) };
}

std::unexpected<Error> Error::fail(
    const Code code, const std::string_view operation, const std::filesystem::path& path, const std::error_code& cause, const std::source_location location)
{
    return std::unexpected<Error> { make(code, operation, path, cause, location) };
}

std::unexpected<Error> Error::fail(const Code code,
    const std::string_view operation,
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    const std::error_code& cause,
    const std::source_location location)
{
    return std::unexpected<Error> { make(code, operation, source, destination, cause, location) };
}

std::unexpected<Error> Error::propagate(Error&& error, std::string message, const std::source_location location)
{
    return std::unexpected<Error> { std::move(error).with_context(std::move(message), location) };
}

void Error::raise(const Code code, std::string message, const std::source_location location) { throw Exception(make(code, std::move(message), location)); }

void Error::raise(const Code code, const std::string_view operation, const std::filesystem::path& path, const std::source_location location)
{
    throw Exception(make(code, operation, path, location));
}

void Error::raise(const Code code, const std::string_view operation, const std::error_code& cause, const std::source_location location)
{
    throw Exception(make(code, operation, cause, location));
}

void Error::raise(
    const Code code, const std::string_view operation, const std::filesystem::path& path, const std::error_code& cause, const std::source_location location)
{
    throw Exception(make(code, operation, path, cause, location));
}

void Error::raise(const Code code,
    const std::string_view operation,
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    const std::error_code& cause,
    const std::source_location location)
{
    throw Exception(make(code, operation, source, destination, cause, location));
}

void Error::panic(const Error& error, const std::source_location location)
{
    PANIC(fmt::format("unexpected failure at {}:{}\n{}\nError origin:\n{}", location.file_name(), location.line(), error.to_string(), error.stacktrace()));
    std::abort();
}

Error::Exception::Exception(Error error)
    : m_error(std::move(error))
    , m_what(m_error.to_string())
{
}

Error Error::with_context(std::string message, const std::source_location location) &&
{
    Error result = std::move(*this);
    result.m_frames.push_back(Frame {
        std::move(message),
        location,
    });
    return result;
}

Error Error::reclassified(const Code code, std::string message, const std::source_location location) &&
{
    Error result = std::move(*this);
    result.m_frames.push_back(Frame {
        fmt::format("{} (reclassified {} -> {})", message, code_name(result.m_code), code_name(code)),
        location,
    });
    result.m_code = code;
    return result;
}

std::string Error::to_string() const
{
    std::string result = fmt::format("[{}]", code_name(m_code));
    for (auto frame = m_frames.rbegin(); frame != m_frames.rend(); ++frame) {
        result += fmt::format("\n{}{}\n  at {}:{} ({})",
            frame == m_frames.rbegin() ? "" : "caused by: ",
            frame->message,
            frame->location.file_name(),
            frame->location.line(),
            frame->location.function_name());
    }
    return result;
}

std::string Error::stacktrace() const { return m_stacktrace ? m_stacktrace->resolve().to_string() : std::string {}; }

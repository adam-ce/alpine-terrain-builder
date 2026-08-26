#include "Error.h"

#include <utility>

#include <fmt/format.h>

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
    case Error::Code::Internal:
        return "Internal";
    }
    return "Unknown";
}

std::string describe_system_error(const std::error_code& cause)
{
    return fmt::format("{} ({}:{})", cause.message(), cause.category().name(), cause.value());
}

} // namespace

Error::Error(const Code code, Frame frame)
    : m_code(code)
{
    m_frames.push_back(std::move(frame));
}

Error Error::make(const Code code, std::string message, const std::source_location location)
{
    return Error(code, Frame { std::move(message), location, std::nullopt, std::nullopt });
}

Error Error::make(const Code code,
    const std::string_view operation,
    const std::filesystem::path& path,
    const std::source_location location)
{
    return make(code, fmt::format("{} \"{}\"", operation, path.string()), location);
}

Error Error::make(const Code code,
    const std::string_view operation,
    const std::error_code& cause,
    const std::source_location location)
{
    return make(code, fmt::format("{}: {}", operation, describe_system_error(cause)), location);
}

Error Error::make(const Code code,
    const std::string_view operation,
    const std::filesystem::path& path,
    const std::error_code& cause,
    const std::source_location location)
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
    return make(code,
        fmt::format("{} \"{}\" -> \"{}\": {}", operation, source.string(), destination.string(), describe_system_error(cause)),
        location);
}

Error Error::with_context(std::string message, const std::source_location location) &&
{
    m_frames.push_back(Frame {
        std::move(message),
        location,
        std::nullopt,
        std::nullopt,
    });
    return std::move(*this);
}

Error Error::reclassified(const Code code, std::string message, const std::source_location location) &&
{
    m_frames.push_back(Frame {
        std::move(message),
        location,
        m_code,
        code,
    });
    m_code = code;
    return std::move(*this);
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
        if (frame->original_code.has_value() && frame->new_code.has_value()) {
            result += fmt::format("\n  reclassified {} -> {}", code_name(frame->original_code.value()), code_name(frame->new_code.value()));
        }
    }
    return result;
}

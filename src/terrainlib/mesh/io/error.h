#pragma once

#include <string>

namespace mesh::io {

enum class LoadMeshErrorKind {
    UnsupportedFormat,
    FileNotFound,
    InvalidFormat,
    OutOfMemory
};

class LoadMeshError {
public:
    LoadMeshError() = default;
    constexpr LoadMeshError(LoadMeshErrorKind kind)
        : kind(kind) {}

    operator LoadMeshErrorKind() const {
        return this->kind;
    }
    constexpr bool operator==(LoadMeshError other) const {
        return this->kind == other.kind;
    }
    constexpr bool operator!=(LoadMeshError other) const {
        return this->kind != other.kind;
    }
    constexpr bool operator==(LoadMeshErrorKind other) const {
        return this->kind == other;
    }
    constexpr bool operator!=(LoadMeshErrorKind other) const {
        return this->kind != other;
    }

    const char* description() const {
        switch (kind) {
            case LoadMeshErrorKind::UnsupportedFormat:
                return "format not supported";
            case LoadMeshErrorKind::FileNotFound:
                return "file not found";
            case LoadMeshErrorKind::InvalidFormat:
                return "invalid file format";
            case LoadMeshErrorKind::OutOfMemory:
                return "out of memory";
            default:
                return "undefined error";
        }
    }

private:
    LoadMeshErrorKind kind;
};

enum class SaveMeshErrorKind {
    UnsupportedFormat,
    OpenFile,
    WriteFile,
    OutOfMemory
};

class SaveMeshError {
public:
    SaveMeshError() = default;
    constexpr SaveMeshError(SaveMeshErrorKind kind)
        : kind(kind) {}

    operator SaveMeshErrorKind() const {
        return this->kind;
    }
    constexpr bool operator==(SaveMeshError other) const {
        return this->kind == other.kind;
    }
    constexpr bool operator!=(SaveMeshError other) const {
        return this->kind != other.kind;
    }
    constexpr bool operator==(SaveMeshErrorKind other) const {
        return this->kind == other;
    }
    constexpr bool operator!=(SaveMeshErrorKind other) const {
        return this->kind != other;
    }

    const char* description() const {
        switch (kind) {
            case SaveMeshErrorKind::UnsupportedFormat:
                return "format not supported";
            case SaveMeshErrorKind::OpenFile:
                return "failed to open output file";
            case SaveMeshErrorKind::WriteFile:
                return "failed to write to output file";
            case SaveMeshErrorKind::OutOfMemory:
                return "out of memory";
            default:
                return "undefined error";
        }
    }

private:
    SaveMeshErrorKind kind;
};

} // namespace mesh::io

#include <fmt/format.h>
template <>
struct fmt::formatter<mesh::io::SaveMeshError> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const mesh::io::SaveMeshError &error, FormatContext &ctx) {
        return fmt::format_to(ctx.out(), "{}", error.description());
    }
};

template <>
struct fmt::formatter<mesh::io::LoadMeshError> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const mesh::io::LoadMeshError &error, FormatContext &ctx) {
        return fmt::format_to(ctx.out(), "{}", error.description());
    }
};

#include <fmt/ostream.h>
#include <iostream>
inline std::ostream &operator<<(std::ostream &os, const mesh::io::SaveMeshError &error) {
    fmt::print(os, "{}", error);
    return os;
}

inline std::ostream &operator<<(std::ostream &os, const mesh::io::LoadMeshError &error) {
    fmt::print(os, "{}", error);
    return os;
}

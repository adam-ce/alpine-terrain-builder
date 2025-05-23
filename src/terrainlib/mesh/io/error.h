#pragma once

#include <string>

namespace mesh::io {

// TODO: use same setup as with NodeStatus here

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

    std::string description() const {
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

    friend std::ostream &operator<<(std::ostream &os, const LoadMeshError &err) {
        return os << err.description();
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

    std::string description() const {
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
    
    friend std::ostream &operator<<(std::ostream &os, const SaveMeshError &error) {
        os << error.description();
        return os;
    }

private:
    SaveMeshErrorKind kind;
};

} // namespace mesh::io

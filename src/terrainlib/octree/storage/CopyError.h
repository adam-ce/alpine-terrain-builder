#pragma once

#include <ostream>
#include <string>

namespace octree {

enum class CopyErrorKind {
    FileNotFound,
    CreateLink,
    CreateDirectories,
    RemoveOld,
    Read,
    Write
};

class CopyError {
public:
    constexpr CopyError(CopyErrorKind kind)
        : kind(kind) {}

    operator CopyErrorKind() const {
        return this->kind;
    }
    constexpr bool operator==(CopyError other) const {
        return this->kind == other.kind;
    }
    constexpr bool operator!=(CopyError other) const {
        return this->kind != other.kind;
    }

    std::string description() const {
        switch (kind) {
        case CopyErrorKind::FileNotFound:
            return "file not found";
        case CopyErrorKind::CreateLink:
            return "cannot create hard link";
        case CopyErrorKind::CreateDirectories:
            return "cannot create directories";
        case CopyErrorKind::RemoveOld:
            return "cannot remove current file";
        case CopyErrorKind::Read:
            return "cannot read file";
        case CopyErrorKind::Write:
            return "cannot write file";
        default:
            return "undefined error";
        }
    }

    friend std::ostream &operator<<(std::ostream &os, const CopyError &err) {
        return os << err.description();
    }

private:
    CopyErrorKind kind;
};

} // namespace octree

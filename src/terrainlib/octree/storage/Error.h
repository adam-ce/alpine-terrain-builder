#pragma once

#include <ostream>
#include <string>

namespace octree {

enum class CopyMeshErrorKind {
    FileNotFound,
    CreateLink,
    CreateDirectories,
    RemoveOld,
    Read,
    Write
};

class CopyMeshError {
public:
    constexpr CopyMeshError(CopyMeshErrorKind kind)
        : kind(kind) {}

    operator CopyMeshErrorKind() const {
        return this->kind;
    }
    constexpr bool operator==(CopyMeshError other) const {
        return this->kind == other.kind;
    }
    constexpr bool operator!=(CopyMeshError other) const {
        return this->kind != other.kind;
    }

    std::string description() const {
        switch (kind) {
        case CopyMeshErrorKind::FileNotFound:
            return "file not found";
        case CopyMeshErrorKind::CreateLink:
            return "cannot create hard link";
        case CopyMeshErrorKind::CreateDirectories:
            return "cannot create directories";
        case CopyMeshErrorKind::RemoveOld:
            return "cannot remove current file";
        default:
            return "undefined error";
        }
    }

    friend std::ostream &operator<<(std::ostream &os, const CopyMeshError &err) {
        return os << err.description();
    }

private:
    CopyMeshErrorKind kind;
};

} // namespace octree

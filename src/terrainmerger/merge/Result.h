#pragma once

#include <variant>

#include "mesh/SimpleMesh.h"

namespace merge {

struct Recurse {};

struct Ignore {};

struct Unchanged {
    bool is_left;
};

struct Merged {
    SimpleMesh mesh;
};

using Result = std::variant<Recurse, Ignore, Unchanged, Merged>;

} // namespace merge

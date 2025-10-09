#pragma once

#include <variant>

#include "mesh/SimpleMesh.h"

namespace merge {

template <typename Context>
struct Recurse {
    // TODO: make optional?
    Context context;
};
template <typename Context>
Recurse(Context) -> Recurse<Context>;

struct Ignore {};

enum class Source {
    Left,
    Right
};

struct Unchanged {
    Source source;
};

struct Merged {
    SimpleMesh mesh;
};

template <typename Context>
using Result = std::variant<Recurse<Context>, Ignore, Unchanged, Merged>;

} // namespace merge

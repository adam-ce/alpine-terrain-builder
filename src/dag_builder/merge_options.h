#pragma once

#include "uv/unwrap.h"

struct MergeOptions {
    uv::Algorithm uv_unwrap_algorithm = uv::DEFAULT_ALGORITHM;
    bool allow_texture_reuse = true; // adopt a group's shared source texture instead of unwrapping
};

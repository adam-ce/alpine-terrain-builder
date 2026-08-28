#pragma once

#include <expected>

#include <libassert/assert.hpp>

#include "mesh/storage.h"
#include "sf/validate_index.h"

namespace sf {

inline Expected<void> finalize_storage(mesh::storage::Storage& storage)
{
    auto save_result = storage.save_or_create_index();
    if (!save_result.has_value()) {
        return save_result;
    }

    const auto index = storage.index();
    DEBUG_ASSERT(index.has_value());
    return validate_index(index->get());
}

} // namespace sf

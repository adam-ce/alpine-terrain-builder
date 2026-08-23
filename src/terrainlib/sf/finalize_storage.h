#pragma once

#include <expected>

#include <libassert/assert.hpp>

#include "mesh/storage.h"
#include "sf/Error.h"
#include "sf/validate_index.h"

namespace sf {

inline std::expected<void, FinalizeError> finalize_storage(mesh::storage::Storage& storage)
{
    const auto save_result = storage.save_or_create_index();
    if (!save_result.has_value()) {
        return std::unexpected(FinalizeError(save_result.error()));
    }

    const auto index = storage.index();
    DEBUG_ASSERT(index.has_value());
    const auto validation = validate_index(index->get());
    if (!validation.has_value()) {
        return std::unexpected(FinalizeError(validation.error()));
    }
    return {};
}

} // namespace sf

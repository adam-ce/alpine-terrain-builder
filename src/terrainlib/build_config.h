#pragma once

#define ALP_UNUSED(value) static_cast<void>(value)

#ifdef NDEBUG
constexpr bool IS_DEBUG_BUILD = false;
#else
constexpr bool IS_DEBUG_BUILD = true;
#endif

constexpr bool IS_RELEASE_BUILD = !IS_DEBUG_BUILD;

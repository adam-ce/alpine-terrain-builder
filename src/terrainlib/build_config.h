#pragma once

#ifdef NDEBUG
constexpr bool IS_DEBUG_BUILD = false;
#else
constexpr bool IS_DEBUG_BUILD = true;
#endif

constexpr bool IS_RELEASE_BUILD = !IS_DEBUG_BUILD;

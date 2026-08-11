#pragma once

#define ALP_UNUSED(...) static_cast<void>([](auto &&...) {}(__VA_ARGS__))

#ifdef NDEBUG
constexpr bool IS_DEBUG_BUILD = false;
#else
constexpr bool IS_DEBUG_BUILD = true;
#endif

constexpr bool IS_RELEASE_BUILD = !IS_DEBUG_BUILD;

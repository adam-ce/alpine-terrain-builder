cmake_minimum_required(VERSION 3.21)

project(alpine-terrain-builder)
option(ALP_UNITTESTS "include unit test targets in the buildsystem" ON)

# ASAN & TSAN
option(ALP_ENABLE_THREAD_SANITIZER "compiles with thread sanitizer enabled" OFF)
option(ALP_ENABLE_ADDRESS_SANITIZER "compiles with address sanitizer enabled" OFF)

if(ALP_ENABLE_ADDRESS_SANITIZER)
    if(ALP_ENABLE_THREAD_SANITIZER)
        message(FATAL_ERROR "Cannot enable both AddressSanitizer and ThreadSanitizer")
    endif()
endif()

if(ALP_ENABLE_ADDRESS_SANITIZER)
    message(STATUS "AddressSanitizer enabled")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(ASAN_FLAGS -fsanitize=address -fsanitize=leak -fsanitize=undefined -fno-omit-frame-pointer -g)
        add_compile_options(${ASAN_FLAGS})
        add_link_options(${ASAN_FLAGS})
    else()
        message(WARNING "ALP_ENABLE_ADDRESS_SANITIZER set but compiler is not GCC/Clang; ignoring")
    endif()
endif()

if(ALP_ENABLE_THREAD_SANITIZER)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        message(WARNING "ThreadSanitizer is not recommended with Release builds. "
                        "Use Debug or RelWithDebInfo for accurate race detection.")
    endif()
    message(STATUS "ThreadSanitizer enabled")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(TSAN_FLAGS -fsanitize=thread -fno-omit-frame-pointer -fno-optimize-sibling-calls -g)
        add_compile_options(${TSAN_FLAGS})
        add_link_options(${TSAN_FLAGS})
    else()
        message(WARNING "ALP_ENABLE_THREAD_SANITIZER set but compiler is not GCC/Clang; ignoring")
    endif()
endif()

include(cmake/AddRepo.cmake)
include(cmake/SetupCMakeProject.cmake)


add_subdirectory(src)
if (ALP_UNITTESTS)
    add_subdirectory(unittests)
endif()

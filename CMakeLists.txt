cmake_minimum_required(VERSION 3.21)

project(alpine-terrain-builder)
option(ALP_UNITTESTS "include unit test targets in the buildsystem" ON)

include(cmake/AddRepo.cmake)


add_subdirectory(src)
if (ALP_UNITTESTS)
    add_subdirectory(unittests)
endif()

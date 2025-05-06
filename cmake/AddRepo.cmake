#############################################################################
# AlpineMaps.org
# Copyright (C) 2023 Adam Celarek <family name at cg tuwien ac at>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#############################################################################

find_package(Git 2.22 REQUIRED)

# CMake's FetchContent caches information about the downloads / clones in the build dir.
# Therefore it walks over the clones every time we switch the build type (release, debug, webassembly, android etc),
# which takes forever. Moreover, it messes up changes to subprojects. This function, on the other hand, checks whether
# we are on a branch and in that case only issues a warning. Use origin/main or similar, if you want to stay up-to-date
# with upstream.

if(NOT DEFINED _alp_add_repo_check_flag)
    set_property(GLOBAL PROPERTY _alp_add_repo_check_flag FALSE)
endif()

function(_alp_git_checkout_branch repo repo_dir commitish)
    message(STATUS "[alp/git] In ${repo}, checking out ${commitish}.")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} checkout --quiet ${commitish}
        WORKING_DIRECTORY ${repo_dir}
        RESULT_VARIABLE GIT_CHECKOUT_RESULT
    )
    if (NOT GIT_CHECKOUT_RESULT)
        message(STATUS "[alp/git] In ${repo}, checking out branch ${commitish} was successfull.")
    else()
        message(FATAL_ERROR "[alp/git] In ${repo}, checking out branch ${commitish} was NOT successfull!")
    endif()

    if (EXISTS "${repo_dir}/.gitmodules")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
            WORKING_DIRECTORY ${repo_dir}
            RESULT_VARIABLE GIT_SUBMODULE_RESULT
        )
        if(GIT_SUBMODULE_RESULT EQUAL 0)
            message(STATUS "[alp/git] In ${repo}, submodules updated to match ${commitish}.")
        else()
            message(WARNING "[alp/git] In ${repo}, submodule update failed after checking out ${commitish}.")
        endif()
    endif()
endfunction()

function(alp_add_git_repository name)
    set(options DO_NOT_ADD_SUBPROJECT NOT_SYSTEM PRIVATE_DO_NOT_CHECK_FOR_SCRIPT_UPDATES)
    set(oneValueArgs URL COMMITISH DESTINATION_PATH)
    set(multiValueArgs )
    cmake_parse_arguments(PARSE_ARGV 1 PARAM "${options}" "${oneValueArgs}" "${multiValueArgs}")

    get_property(_check_ran GLOBAL PROPERTY _alp_add_repo_check_flag)
    if(NOT PARAM_PRIVATE_DO_NOT_CHECK_FOR_SCRIPT_UPDATES AND NOT _check_ran)
        if(NOT COMMAND alp_check_for_script_updates)
            include(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CheckForScriptUpdates.cmake)
        endif()
        alp_check_for_script_updates(${CMAKE_CURRENT_FUNCTION_LIST_FILE})
        set_property(GLOBAL PROPERTY _alp_add_repo_check_flag TRUE)
    endif()

    if (NOT DEFINED ALP_EXTERN_DIR OR ALP_EXTERN_DIR STREQUAL "")
        set(ALP_EXTERN_DIR extern)
    endif()

    set(repo_dir ${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/${name})
    set(short_repo_dir ${ALP_EXTERN_DIR}/${name})
    if (DEFINED PARAM_DESTINATION_PATH AND NOT PARAM_DESTINATION_PATH STREQUAL "")
        set(repo_dir ${CMAKE_SOURCE_DIR}/${PARAM_DESTINATION_PATH})
        set(short_repo_dir ${PARAM_DESTINATION_PATH})
    endif()
    file(MAKE_DIRECTORY ${repo_dir})

    set(${name}_SOURCE_DIR "${repo_dir}" PARENT_SCOPE)

    string(REGEX MATCH "^[^/]+/.+" commitish_is_remote_branch "${PARAM_COMMITISH}")

    if(EXISTS "${repo_dir}/.git")
        # First, see if PARAM_COMMITISH is a valid local ref:
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --verify ${PARAM_COMMITISH}
            WORKING_DIRECTORY ${repo_dir}
            OUTPUT_VARIABLE GIT_COMMIT_OUTPUT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE GIT_COMMIT_RESULT
        )

        if (GIT_COMMIT_RESULT EQUAL 0 AND NOT commitish_is_remote_branch)
            # PARAM_COMMITISH is recognized by Git => no need to fetch
            # (could be a tag (lightweight or annotated) or a direct commit SHA).
            # if it's an *annotated* tag, rev-parse gives us the tag object's hash, not the commit hash.
            # => Force resolve the actual commit object with ^{commit}:
            execute_process(
                COMMAND ${GIT_EXECUTABLE} rev-parse --verify ${PARAM_COMMITISH}^{commit}
                WORKING_DIRECTORY ${repo_dir}
                OUTPUT_VARIABLE GIT_COMMIT_OBJECT
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE GIT_COMMIT_OBJECT_RESULT
            )

            if (GIT_COMMIT_OBJECT_RESULT EQUAL 0)
                # Successfully resolved a commit object
                set(CHECK_COMMITISH "${GIT_COMMIT_OBJECT}")
            else()
                # Fallback if that fails (should rarely happen if it's a proper commit/tag)
                set(CHECK_COMMITISH "${GIT_COMMIT_OUTPUT}")
            endif()

            # Grab HEAD commit
            execute_process(
                COMMAND ${GIT_EXECUTABLE} rev-parse --verify HEAD
                WORKING_DIRECTORY ${repo_dir}
                OUTPUT_VARIABLE GIT_HEAD_OUTPUT
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )

            if (GIT_HEAD_OUTPUT STREQUAL CHECK_COMMITISH)
                message(STATUS "[alp/git] ${short_repo_dir} is already at ${PARAM_COMMITISH}. Skipping checkout.")
            else()
                _alp_git_checkout_branch(${short_repo_dir} ${repo_dir} ${PARAM_COMMITISH})
            endif()
        else()
            # either remote branch or commitish not recognised
            message(STATUS "[alp/git] Fetching updates for ${short_repo_dir}.")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} fetch
                WORKING_DIRECTORY ${repo_dir}
                RESULT_VARIABLE GIT_FETCH_RESULT
            )
            if (GIT_FETCH_RESULT EQUAL 0)
                message(STATUS "[alp/git] Fetch successful for ${short_repo_dir}.")
                execute_process(
                    COMMAND ${GIT_EXECUTABLE} branch --show-current
                    WORKING_DIRECTORY ${repo_dir}
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    OUTPUT_VARIABLE GIT_BRANCH_OUTPUT
                    RESULT_VARIABLE GIT_BRANCH_RESULT
                )
                if (NOT GIT_BRANCH_RESULT EQUAL 0)
                    message(FATAL_ERROR "[alp/git] In ${short_repo_dir}, git branch --show-current not successfull")
                endif()

                if (GIT_BRANCH_OUTPUT STREQUAL "")
                    # Currently detached; let's checkout the branch
                    _alp_git_checkout_branch(${short_repo_dir} ${repo_dir} ${PARAM_COMMITISH})
                else()
                    message(WARNING
                        "[alp/git] ${short_repo_dir} on branch ${GIT_BRANCH_OUTPUT}, leaving it there. "
                        "NOT checking out ${PARAM_COMMITISH}! Use origin/main or similar if you "
                        "want to stay up-to-date with upstream."
                    )
                endif()
            else ()
                message(WARNING "[alp/git] Not able to fetch updates for ${short_repo_dir} and ${PARAM_COMMITISH} was not found locally or is a remote branch.")
            endif()
        endif()
    else()
        # If the repo doesn't exist, do a fresh clone
        message(STATUS "[alp/git] Cloning ${PARAM_URL} to ${repo_dir}.")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone --recurse-submodules ${PARAM_URL} ${repo_dir}
            RESULT_VARIABLE GIT_CLONE_RESULT
        )
        if (GIT_CLONE_RESULT EQUAL 0)
            _alp_git_checkout_branch(${short_repo_dir} ${repo_dir} ${PARAM_COMMITISH})
        else()
            message(FATAL_ERROR "[alp/git] Cloning ${short_repo_dir} was NOT successfull!")
        endif()
    endif()

    if (NOT ${PARAM_DO_NOT_ADD_SUBPROJECT})
        if (NOT ${PARAM_NOT_SYSTEM})
            add_subdirectory(${repo_dir} ${CMAKE_BINARY_DIR}/alp_external/${name} SYSTEM)
        else()
            add_subdirectory(${repo_dir} ${CMAKE_BINARY_DIR}/alp_external/${name})
        endif()
    endif()
endfunction()

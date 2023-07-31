# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(cxxopts_REV "3f2d70530219e09fe7e563f86126b0d3b228a60d")

# UE Change Begin: Don't link CMake script with git to avoid mess with side branches.
#UpdateExternalLib("cxxopts" "https://github.com/jarro2783/cxxopts.git" ${cxxopts_REV})
# UE Change End: Don't link CMake script with git to avoid mess with side branches.

set(CXXOPTS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CXXOPTS_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(cxxopts EXCLUDE_FROM_ALL)

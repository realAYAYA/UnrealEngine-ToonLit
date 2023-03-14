# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(googletest_REV "dcc92d0ab6c4ce022162a23566d44f673251eee4")

# UE Change Begin: Don't link CMake script with git to avoid mess with side branches.
#UpdateExternalLib("googletest" "https://github.com/google/googletest.git" ${googletest_REV})
# UE Change End: Don't link CMake script with git to avoid mess with side branches.

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(googletest EXCLUDE_FROM_ALL)
foreach(target
    "gtest" "gtest_main")
    set_target_properties(${target} PROPERTIES FOLDER "External/googletest")
endforeach()

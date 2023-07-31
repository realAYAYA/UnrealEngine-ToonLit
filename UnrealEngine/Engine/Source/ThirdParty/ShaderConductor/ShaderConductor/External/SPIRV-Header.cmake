# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(SPIRV_Headers_REV "3fdabd0da2932c276b25b9b4a988ba134eba1aa6")

# UE Change Begin: Don't link CMake script with git to avoid mess with side branches.
#UpdateExternalLib("SPIRV-Headers" "https://github.com/KhronosGroup/SPIRV-Headers.git" ${SPIRV_Headers_REV})
# UE Change End: Don't link CMake script with git to avoid mess with side branches.

add_subdirectory(SPIRV-Headers EXCLUDE_FROM_ALL)
foreach(target
    "install-headers" "SPIRV-Headers-example")
    set_target_properties(${target} PROPERTIES FOLDER "External/SPIRV-Headers")
endforeach()

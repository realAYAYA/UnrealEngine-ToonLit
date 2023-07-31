# Copyright Epic Games, Inc. All Rights Reserved.

set(CMAKE_SYSTEM_PROCESSOR    x86_64)
set(CMAKE_C_COMPILER_TARGET   x86_64-unknown-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET x86_64-unknown-linux-gnu)

# Required for PlatformScripts/Linux/Linux to be found.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../..")
include(PlatformScripts/Linux/Linux)

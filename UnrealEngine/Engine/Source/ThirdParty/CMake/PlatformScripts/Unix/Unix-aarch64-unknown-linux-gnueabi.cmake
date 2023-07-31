# Copyright Epic Games, Inc. All Rights Reserved.

set(CMAKE_SYSTEM_PROCESSOR    aarch64)
set(CMAKE_C_COMPILER_TARGET   aarch64-unknown-linux-gnueabi)
set(CMAKE_CXX_COMPILER_TARGET aarch64-unknown-linux-gnueabi)

# Required for PlatformScripts/Unix/Unix to be found.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../..")
include(PlatformScripts/Unix/Unix)

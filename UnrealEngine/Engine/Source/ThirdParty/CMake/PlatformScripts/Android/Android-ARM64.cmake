# Copyright Epic Games, Inc. All Rights Reserved.

set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)

# Required for PlatformScripts/Android/Android to be found.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../..")
include(PlatformScripts/Android/Android)

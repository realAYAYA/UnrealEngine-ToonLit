# Copyright Epic Games, Inc. All Rights Reserved.

set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 21)
set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE STRING "" FORCE)

if(NOT DEFINED ENV{ANDROID_NDK_ROOT})
	message(FATAL_ERROR "ANDROID_NDK_ROOT environment variable is not set!")
endif()

file(TO_CMAKE_PATH $ENV{ANDROID_NDK_ROOT} CMAKE_ANDROID_NDK)

if(NOT EXISTS ${CMAKE_ANDROID_NDK})
	message(FATAL_ERROR "ANDROID_NDK_ROOT environment variable must point to the NDK directory!")
endif()

if(NOT EXISTS "${CMAKE_ANDROID_NDK}/meta/platforms.json")
	message(FATAL_ERROR "Android NDK at ${CMAKE_ANDROID_NDK} does not contain meta/platforms.json!")
endif()
file(READ "${CMAKE_ANDROID_NDK}/meta/platforms.json" NDK_PLATFORMS)
string(JSON NDK_API_MIN GET "${NDK_PLATFORMS}" "min")
string(JSON NDK_API_MAX GET "${NDK_PLATFORMS}" "max")
if((CMAKE_SYSTEM_VERSION LESS NDK_API_MIN) OR (NDK_API_MAX LESS CMAKE_SYSTEM_VERSION))
	message(FATAL_ERROR "Android NDK at ${CMAKE_ANDROID_NDK} supports versions ${NDK_API_MIN}-${NDK_API_MAX} and we require version ${CMAKE_SYSTEM_VERSION}!")
endif()

string(CONCAT UE_FLAGS
	" -fPIC"
	" -fno-short-enums"
	" -fno-strict-aliasing"
	" -fstack-protector-strong"
	" -funwind-tables"
	" -g2"
	" -gdwarf-4"
	" -no-canonical-prefixes"
	)

string(CONCAT UE_FLAGS_C
	)

string(CONCAT UE_FLAGS_CXX
	" -std=c++14"
	)

string(CONCAT UE_FLAGS_DEBUG
	" -O0"
	" -D_DEBUG"
	" -DDEBUG"
	)

string(CONCAT UE_FLAGS_RELEASE
	" -O3"
	" -DNDEBUG"
	)

set(CMAKE_C_FLAGS           "${UE_FLAGS} ${UE_FLAGS_C}"   CACHE STRING "C Flags"           FORCE)
set(CMAKE_CXX_FLAGS         "${UE_FLAGS} ${UE_FLAGS_CXX}" CACHE STRING "C++ Flags"         FORCE)
set(CMAKE_C_FLAGS_DEBUG     "${UE_FLAGS_DEBUG}"           CACHE STRING "C Debug Flags"     FORCE)
set(CMAKE_CXX_FLAGS_DEBUG   "${UE_FLAGS_DEBUG}"           CACHE STRING "C++ Debug Flags"   FORCE)
set(CMAKE_C_FLAGS_RELEASE   "${UE_FLAGS_RELEASE}"         CACHE STRING "C Release Flags"   FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "${UE_FLAGS_RELEASE}"         CACHE STRING "C++ Release Flags" FORCE)

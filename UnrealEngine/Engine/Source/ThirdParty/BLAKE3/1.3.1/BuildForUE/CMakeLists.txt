# Copyright Epic Games, Inc. All Rights Reserved.

cmake_minimum_required(VERSION 3.11)
project(BLAKE3)

if(MSVC)
	enable_language(C ASM_MASM)
else()
	enable_language(C ASM)
endif()

option(BLAKE3_NO_SSE2 "Disable SSE2 on x86" OFF)
option(BLAKE3_NO_SSE41 "Disable SSE4.1 on x86" OFF)
option(BLAKE3_NO_AVX2 "Disable AVX2 on x86" OFF)
option(BLAKE3_NO_AVX512 "Disable AVX-512 on x86" OFF)
option(BLAKE3_NO_NEON "Disable NEON on ARM" OFF)



if(MSVC)
	set(ARCH_ARGS -EP)
else()
	set(ARCH_ARGS -E -P)
	if(CMAKE_C_COMPILER_TARGET)
		set(ARCH_ARGS ${ARCH_ARGS} --target=${CMAKE_C_COMPILER_TARGET})
	elseif(CMAKE_OSX_ARCHITECTURES)
		set(ARCH_ARGS ${ARCH_ARGS} -arch ${CMAKE_OSX_ARCHITECTURES})
	endif()
endif()
execute_process(COMMAND ${CMAKE_C_COMPILER} ${ARCH_ARGS} "${CMAKE_CURRENT_LIST_DIR}/CMakeArchitecture.c"
	RESULT_VARIABLE BLAKE3_ARCH_EXIT_CODE
	OUTPUT_VARIABLE BLAKE3_ARCH
	ERROR_QUIET)
if(NOT ${BLAKE3_ARCH_EXIT_CODE} EQUAL 0)
	message(FATAL_ERROR "Failed to compile CMakeArchitecture.c to detect the target architecture!")
endif()
string(STRIP ${BLAKE3_ARCH} BLAKE3_ARCH)
if(${BLAKE3_ARCH} STREQUAL "unknown")
	message(FATAL_ERROR "Failed to detect the target architecture in CMakeArchitecture.c!")
endif()
message(STATUS "Architecture: ${BLAKE3_ARCH}")



set(BLAKE3_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

set(BLAKE3_SOURCES
	"${BLAKE3_ROOT}/c/blake3.c"
	"${BLAKE3_ROOT}/c/blake3_dispatch.c"
	"${BLAKE3_ROOT}/c/blake3_portable.c"
	)

function(blake3_add_variant name option prefix msvc_args clang_args)
	if(BLAKE3_NO_${option})
		add_definitions(-DBLAKE3_NO_${option})
		return()
	endif()

	message(STATUS "Using ${name}")

	if(NOT ${BLAKE3_ARCH} STREQUAL "x86_64")
		set(suffix ".c")
	elseif(MSVC)
		set(suffix "_x86-64_windows_msvc.asm")
	elseif(WIN32)
		set(suffix "_x86-64_windows_gnu.S")
	else()
		set(suffix "_x86-64_unix.S")
	endif()

	set(path "${BLAKE3_ROOT}/c/blake3_${prefix}${suffix}")
	set(BLAKE3_SOURCES ${BLAKE3_SOURCES} ${path} PARENT_SCOPE)

	if(NOT ${BLAKE3_ARCH} STREQUAL "x86_64")
		if(MSVC)
			set_property(SOURCE ${path} PROPERTY COMPILE_FLAGS ${msvc_args})
		else()
			set_property(SOURCE ${path} PROPERTY COMPILE_FLAGS ${clang_args})
		endif()
	endif()
endfunction()

if(${BLAKE3_ARCH} STREQUAL "x86_64" OR ${BLAKE3_ARCH} STREQUAL "x86")

	blake3_add_variant("SSE2" SSE2 "sse2" "" "-msse2")
	blake3_add_variant("SSE4.1" SSE41 "sse41" "" "-msse4.1")
	blake3_add_variant("AVX2" AVX2 "avx2" "/arch:AVX2" "-mavx2")
	blake3_add_variant("AVX-512" AVX512 "avx512" "/arch:AVX512" "-mavx512f -mavx512vl")

elseif(${BLAKE3_ARCH} STREQUAL "arm64" OR ${BLAKE3_ARCH} STREQUAL "arm")

	blake3_add_variant("NEON" NEON "neon" "" "")

endif()

if(MSVC)
	add_compile_options(/nologo)
	set(CMAKE_C_FLAGS "")
	set(CMAKE_C_FLAGS_RELEASE "/O2")
endif()

add_library(BLAKE3 ${BLAKE3_SOURCES})

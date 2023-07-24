# Copyright Epic Games, Inc. All Rights Reserved.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE STRING "" FORCE)

option(BUILD_WITH_LIBCXX "Include LibCxx" OFF)

if(NOT DEFINED ENV{LINUX_MULTIARCH_ROOT})
	message(FATAL_ERROR "LINUX_MULTIARCH_ROOT environment variable is not set!")
endif()

file(TO_CMAKE_PATH $ENV{LINUX_MULTIARCH_ROOT} LINUX_MULTIARCH_ROOT)

if(NOT EXISTS ${LINUX_MULTIARCH_ROOT})
	message(FATAL_ERROR "LINUX_MULTIARCH_ROOT environment variable must point to the root directory!")
endif()

if(DEFINED CMAKE_C_COMPILER_TARGET)
	set(UE_LINUX_TARGET ${CMAKE_C_COMPILER_TARGET})
elseif(DEFINED CMAKE_CXX_COMPILER_TARGET)
	set(UE_LINUX_TARGET ${CMAKE_CXX_COMPILER_TARGET})
else()
	message(FATAL_ERROR "Must define CMAKE_C_COMPILER_TARGET or CMAKE_CXX_COMPILER_TARGET to use this toolchain!")
endif()

set(CMAKE_SYSROOT "${LINUX_MULTIARCH_ROOT}/${UE_LINUX_TARGET}")

if(NOT EXISTS ${CMAKE_SYSROOT})
	message(FATAL_ERROR "Target '${UE_LINUX_TARGET}' does not exist in '${LINUX_MULTIARCH_ROOT}'!")
endif()

SET(LIBCXX_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../Unix/LibCxx")

string(CONCAT UE_FLAGS
	" -fPIC"
	" -gdwarf-4"
	" --target=${UE_LINUX_TARGET}" # Needed until commit 270e0d9c to CMake in ~3.18.4.
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

if(BUILD_WITH_LIBCXX)
	string(CONCAT UE_FLAGS_CXX
		"${UE_FLAGS_CXX}"
		" -stdlib=libc++"
		" -I${LIBCXX_PATH}/include"
		" -I${LIBCXX_PATH}/include/c++/v1"
		)
	string(CONCAT LIBCXX_FLAGS
		"${LIBCXX_FLAGS}"
		" -stdlib=libc++"
		" -L${LIBCXX_PATH}/lib/Unix/${UE_LINUX_TARGET}"
		" ${LIBCXX_PATH}/lib/Unix/${UE_LINUX_TARGET}/libc++.a"
		" ${LIBCXX_PATH}/lib/Unix/${UE_LINUX_TARGET}/libc++abi.a"
		)
	set(CMAKE_MODULE_LINKER_FLAGS "${LIBCXX_FLAGS}")
	set(CMAKE_EXE_LINKER_FLAGS "${LIBCXX_FLAGS}")
	set(CMAKE_SHARED_LINKER_FLAGS "${LIBCXX_FLAGS}")
endif()

set(CMAKE_C_FLAGS           "${UE_FLAGS} ${UE_FLAGS_C}"   CACHE STRING "C Flags"           FORCE)
set(CMAKE_CXX_FLAGS         "${UE_FLAGS} ${UE_FLAGS_CXX}" CACHE STRING "C++ Flags"         FORCE)
set(CMAKE_C_FLAGS_DEBUG     "${UE_FLAGS_DEBUG}"           CACHE STRING "C Debug Flags"     FORCE)
set(CMAKE_CXX_FLAGS_DEBUG   "${UE_FLAGS_DEBUG}"           CACHE STRING "C++ Debug Flags"   FORCE)
set(CMAKE_C_FLAGS_RELEASE   "${UE_FLAGS_RELEASE}"         CACHE STRING "C Release Flags"   FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "${UE_FLAGS_RELEASE}"         CACHE STRING "C++ Release Flags" FORCE)

set(CMAKE_ASM_COMPILER_TARGET "${UE_LINUX_TARGET}")
set(CMAKE_C_COMPILER_TARGET   "${UE_LINUX_TARGET}")
set(CMAKE_CXX_COMPILER_TARGET "${UE_LINUX_TARGET}")

set(CMAKE_C_COMPILER   "${CMAKE_SYSROOT}/bin/clang.exe"                      CACHE PATH "C Compiler")
set(CMAKE_CXX_COMPILER "${CMAKE_SYSROOT}/bin/clang++.exe"                    CACHE PATH "C++ Compiler")

set(CMAKE_AR           "${CMAKE_SYSROOT}/bin/llvm-ar.exe"                    CACHE PATH "Archive")
set(CMAKE_AS           "${CMAKE_SYSROOT}/bin/${UE_LINUX_TARGET}-as.exe"      CACHE PATH "Assembler")
set(CMAKE_LINKER       "${CMAKE_SYSROOT}/bin/lld.exe"                        CACHE PATH "Linker")
set(CMAKE_NM           "${CMAKE_SYSROOT}/bin/${UE_LINUX_TARGET}-nm.exe"      CACHE PATH "NM")
set(CMAKE_OBJCOPY      "${CMAKE_SYSROOT}/bin/${UE_LINUX_TARGET}-objcopy.exe" CACHE PATH "ObjCopy")
set(CMAKE_OBJDUMP      "${CMAKE_SYSROOT}/bin/${UE_LINUX_TARGET}-objdump.exe" CACHE PATH "ObjDump")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_SHARED_LIBRARY_PREFIX_C   "lib")
set(CMAKE_SHARED_LIBRARY_PREFIX_CXX "lib")
set(CMAKE_STATIC_LIBRARY_PREFIX_C   "lib")
set(CMAKE_STATIC_LIBRARY_PREFIX_CXX "lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX_C   ".a")
set(CMAKE_STATIC_LIBRARY_SUFFIX_CXX ".a")
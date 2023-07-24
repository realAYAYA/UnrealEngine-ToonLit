# Copyright Epic Games, Inc. All Rights Reserved.

set(CMAKE_SYSTEM_NAME tvOS)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

string(CONCAT UE_FLAGS
	" -gdwarf-2"
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
	" -fembed-bitcode"
	)

set(CMAKE_C_FLAGS           "${UE_FLAGS} ${UE_FLAGS_C}"   CACHE STRING "C Flags"           FORCE)
set(CMAKE_CXX_FLAGS         "${UE_FLAGS} ${UE_FLAGS_CXX}" CACHE STRING "C++ Flags"         FORCE)
set(CMAKE_C_FLAGS_DEBUG     "${UE_FLAGS_DEBUG}"           CACHE STRING "C Debug Flags"     FORCE)
set(CMAKE_CXX_FLAGS_DEBUG   "${UE_FLAGS_DEBUG}"           CACHE STRING "C++ Debug Flags"   FORCE)
set(CMAKE_C_FLAGS_RELEASE   "${UE_FLAGS_RELEASE}"         CACHE STRING "C Release Flags"   FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "${UE_FLAGS_RELEASE}"         CACHE STRING "C++ Release Flags" FORCE)

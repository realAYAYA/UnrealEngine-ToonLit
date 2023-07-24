// Copyright Epic Games, Inc. All Rights Reserved.

/*================================================================================
	ClangPlatform.h: Setup for any Clang-using platform
==================================================================================*/

#pragma once

// HEADER_UNIT_UNSUPPORTED - Clang not supporting header units

#if !defined(__cpp_if_constexpr)
	#error "Compiler is expected to support if constexpr"
#endif

#if defined(__cpp_fold_expressions)
	#define PLATFORM_COMPILER_HAS_FOLD_EXPRESSIONS 1
#else
	#error "Compiler is expected to support fold expressions"
#endif

#define PLATFORM_RETURN_ADDRESS()			__builtin_return_address(0)
#define PLATFORM_RETURN_ADDRESS_POINTER()	__builtin_frame_address(0)

#define UE_LIFETIMEBOUND [[clang::lifetimebound]]

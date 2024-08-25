// Copyright Epic Games, Inc. All Rights Reserved.

/*================================================================================
	MSVCPlatform.h: Setup for any MSVC-using platform
==================================================================================*/

#pragma once

// HEADER_UNIT_SKIP - Not included directly

#if _MSC_VER < 1920
	#error "Compiler is expected to support if constexpr"
#endif

#if !defined(__cpp_fold_expressions)
	#error "Compiler is expected to support fold expressions"
#endif

#define PLATFORM_RETURN_ADDRESS()	        _ReturnAddress()
#define PLATFORM_RETURN_ADDRESS_POINTER()	_AddressOfReturnAddress()

// https://devblogs.microsoft.com/cppblog/improving-the-state-of-debug-performance-in-c/
#if __has_cpp_attribute(msvc::intrinsic)
#define UE_INTRINSIC_CAST [[msvc::intrinsic]]
#endif

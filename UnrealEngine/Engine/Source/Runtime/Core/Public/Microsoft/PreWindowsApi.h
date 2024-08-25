// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/Platform.h"

#if PLATFORM_WINDOWS
	#include "Windows/PreWindowsApi.h" // HEADER_UNIT_IGNORE
#else
	// this file should only be included from WindowsHWrapper.h
	#if !defined(WINDOWS_H_WRAPPER_GUARD) 
	#pragma message("WARNING: do not include Microsoft/PreWindowsApi.h directly. Use Microsoft/WindowsHWrapper.h or Microsoft/AllowMicrosoftPlatformTypes.h instead") 
	#endif

	#include "Microsoft/PreWindowsApiPrivate.h"
#endif

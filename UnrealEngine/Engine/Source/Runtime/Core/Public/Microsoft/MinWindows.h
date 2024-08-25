// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if PLATFORM_WINDOWS
	#include "Windows/MinWindows.h" // HEADER_UNIT_IGNORE
#else
	#if !defined(WINDOWS_H_WRAPPER_GUARD)
	#pragma message("WARNING: do not include Microsoft/MinWindows.h directly. Use Microsoft/WindowsHWrapper.h instead")
	#endif

	#include "Microsoft/MinWindowsPrivate.h"
#endif

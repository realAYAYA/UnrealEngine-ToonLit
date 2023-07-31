// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef AJA_PLATFORM_TYPES_GUARD
	#define AJA_PLATFORM_TYPES_GUARD
#else
	#error Nesting AjaAllowPlatformTypes.h is not allowed!
#endif

#ifndef PLATFORM_WINDOWS
	#include "Processing.AJA.compat.h"
#endif

#define DWORD ::DWORD
#define FLOAT ::FLOAT

#ifndef TRUE
	#define TRUE 1
#endif

#ifndef FALSE
	#define FALSE 0
#endif

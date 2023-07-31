// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef BLACKMAGIC_PLATFORM_TYPES_GUARD
	#define BLACKMAGIC_PLATFORM_TYPES_GUARD
#else
	#error Nesting BlackmagicAllowPlatformTypes.h is not allowed!
#endif

#ifndef PLATFORM_WINDOWS
	#include "Processing.Blackmagic.compat.h"
#endif

#define DWORD ::DWORD
#define FLOAT ::FLOAT

#ifndef TRUE
	#define TRUE 1
#endif

#ifndef FALSE
	#define FALSE 0
#endif

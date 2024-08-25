// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "HAL/Platform.h"

#define VERSE_UNREACHABLE()   \
	do                        \
	{                         \
		while (true)          \
		{                     \
			PLATFORM_BREAK(); \
		}                     \
	}                         \
	while (false)

#endif // WITH_VERSE_VM

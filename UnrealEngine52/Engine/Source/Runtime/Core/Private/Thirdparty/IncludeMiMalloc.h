// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

// HEADER_UNIT_SKIP - Not included directly

#if !defined(PLATFORM_BUILDS_MIMALLOC)
#	define PLATFORM_BUILDS_MIMALLOC 0
#endif

#if PLATFORM_BUILDS_MIMALLOC

#include "HAL/LowLevelMemTrackerDefines.h"

#if LLM_ENABLED_IN_CONFIG
#define MI_USE_EXTERNAL_ALLOCATORS 1
#endif

#define MI_PADDING 0
#define MI_TSAN 0

#ifdef __cplusplus
THIRD_PARTY_INCLUDES_START
#endif
#include <mimalloc.h>
#ifdef __cplusplus
THIRD_PARTY_INCLUDES_END
#endif

#endif // PLATFORM_BUILDS_MIMALLOC

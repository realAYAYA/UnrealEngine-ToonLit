// Copyright Epic Games, Inc. All Rights Reserved.

#include "IncludeMiMalloc.h"

#if PLATFORM_BUILDS_MIMALLOC

#define MI_OSX_ZONE 0
#define TARGET_IOS_IPHONE 0
#define TARGET_IOS_SIMULATOR 0

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4668) // Avoid undefined __cplusplus warnings in older versions
#endif

// Avoid pulling in headers
#if defined(__clang__)
_Pragma("clang diagnostic push") \
_Pragma("clang diagnostic ignored \"-Wimplicit-int-float-conversion\"")
_Pragma("clang diagnostic ignored \"-Wimplicit-int-conversion\"")
_Pragma("clang diagnostic ignored \"-Wdeprecated-pragma\"")
#endif

#include <static.c>

#if defined(__clang__)
_Pragma("clang diagnostic pop")
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // PLATFORM_BUILDS_MIMALLOC

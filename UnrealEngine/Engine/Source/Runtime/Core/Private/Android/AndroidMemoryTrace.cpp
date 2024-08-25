// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/MemoryTrace.h"

#if UE_MEMORY_TRACE_ENABLED

#include <android/log.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
FMalloc* MemoryTrace_CreateInternal(FMalloc*);

////////////////////////////////////////////////////////////////////////////////
FMalloc* MemoryTrace_Create(FMalloc* InMalloc)
{
	const char* UEEnableMemoryTracing = getenv("UEEnableMemoryTracing");
	__android_log_print(ANDROID_LOG_DEBUG, "UE", "getenv(\"UEEnableMemoryTracing\") == \"%s\"", UEEnableMemoryTracing ? UEEnableMemoryTracing : "nullptr");

	const bool bEnableMemoryTracing = UEEnableMemoryTracing != nullptr && !strncmp(UEEnableMemoryTracing, "1", 1);
	return bEnableMemoryTracing ? MemoryTrace_CreateInternal(InMalloc) :  InMalloc;
}

#endif // UE_MEMORY_TRACE_ENABLED

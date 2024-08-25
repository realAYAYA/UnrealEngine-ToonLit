// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformCrashContext.h"

#define CR_MAX_GPU_BREADCRUMBS_QUEUES 2
#define CR_MAX_GPU_BREADCRUMBS_STRING_CHARS 1024*8

/** Fixed size structure holding GPU breadcrumbs information, to be communicated to the crash reporting client. */
struct FGPUBreadcrumbsSharedContext
{
	struct FQueueData {
		TCHAR QueueName[CR_MAX_GENERIC_FIELD_CHARS];
		TCHAR FullHash[CR_MAX_GENERIC_FIELD_CHARS];
		TCHAR ActiveHash[CR_MAX_GENERIC_FIELD_CHARS];
		TCHAR Breadcrumbs[CR_MAX_GPU_BREADCRUMBS_STRING_CHARS];
	};

	TCHAR SourceName[CR_MAX_GENERIC_FIELD_CHARS];
	TCHAR Version[CR_MAX_GENERIC_FIELD_CHARS];

	uint32 NumQueues = 0;
	FQueueData Queues[CR_MAX_GPU_BREADCRUMBS_QUEUES];
};

/**
 * Fixed size struct that extends FSharedCrashContext with GPU breadcrumbs information.
 */
struct FSharedCrashContextEx : public FSharedCrashContext
{
	FGPUBreadcrumbsSharedContext GPUBreadcrumbs;
};

CORE_API void CopyGPUBreadcrumbsToSharedCrashContext(FSharedCrashContextEx& InOutSharedContext);
CORE_API void InitializeFromCrashContextEx(const FSessionContext& Context, const TCHAR* EnabledPlugins, const TCHAR* EngineData, const TCHAR* GameData, const FGPUBreadcrumbsSharedContext* GPUBreadcrumbs);
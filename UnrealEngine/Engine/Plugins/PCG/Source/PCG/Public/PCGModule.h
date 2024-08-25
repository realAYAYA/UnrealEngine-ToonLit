// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stats/Stats.h"

// Logs
PCG_API DECLARE_LOG_CATEGORY_EXTERN(LogPCG, Log, All);

struct FPCGContext;

namespace PCGLog
{
	/** Convenience function that would either log error on the graph if there is a context, or in the console if not. */
	PCG_API void LogErrorOnGraph(const FText& InMsg, const FPCGContext* InContext = nullptr);
	/** Convenience function that would either log warning on the graph if there is a context, or in the console if not. */
	PCG_API void LogWarningOnGraph(const FText& InMsg, const FPCGContext* InContext = nullptr);
}

namespace PCGEngineShowFlags
{
	static constexpr TCHAR Debug[] = TEXT("PCGDebug");
}

// Stats
DECLARE_STATS_GROUP(TEXT("PCG"), STATGROUP_PCG, STATCAT_Advanced);

// CVars

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Stats/Stats.h"
#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProfilingDebugging/CsvProfiler.h"

CHAOS_API DECLARE_LOG_CATEGORY_EXTERN(LogChaosGeneral, Log, All);
CHAOS_API DECLARE_LOG_CATEGORY_EXTERN(LogChaosThread, Log, All);
CHAOS_API DECLARE_LOG_CATEGORY_EXTERN(LogChaosSimulation, Log, All);
CHAOS_API DECLARE_LOG_CATEGORY_EXTERN(LogChaosDebug, Log, All);

CHAOS_API DECLARE_LOG_CATEGORY_EXTERN(LogChaos, Log, All);

CSV_DECLARE_CATEGORY_MODULE_EXTERN(CHAOS_API, Chaos);

CSV_DECLARE_CATEGORY_MODULE_EXTERN(CHAOS_API, PhysicsVerbose);
CSV_DECLARE_CATEGORY_MODULE_EXTERN(CHAOS_API, PhysicsCounters);
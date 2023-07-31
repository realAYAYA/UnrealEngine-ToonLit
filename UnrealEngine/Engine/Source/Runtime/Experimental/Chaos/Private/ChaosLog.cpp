// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosLog.h"

#include "ChaosModule.h"
#include "CoreMinimal.h"
#include "ProfilingDebugging/CsvProfiler.h"

DEFINE_LOG_CATEGORY(LogChaosGeneral);
DEFINE_LOG_CATEGORY(LogChaosThread);
DEFINE_LOG_CATEGORY(LogChaosSimulation);
DEFINE_LOG_CATEGORY(LogChaosDebug);
DEFINE_LOG_CATEGORY(LogChaos);

CSV_DEFINE_CATEGORY_MODULE(CHAOS_API, Chaos, true);
CSV_DEFINE_CATEGORY_MODULE(CHAOS_API, PhysicsVerbose, false);
CSV_DEFINE_CATEGORY_MODULE(CHAOS_API, PhysicsCounters, false);
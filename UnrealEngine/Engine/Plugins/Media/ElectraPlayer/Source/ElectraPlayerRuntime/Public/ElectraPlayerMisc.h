// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "ProfilingDebugging/CsvProfiler.h"

/** Log category for the ElectraPlayer module. */
DECLARE_LOG_CATEGORY_EXTERN(LogElectraPlayer, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogElectraPlayerStats, Log, All);

/** CSV Profiler */
CSV_DECLARE_CATEGORY_MODULE_EXTERN(ELECTRAPLAYERRUNTIME_API, MediaStreaming);
CSV_DECLARE_CATEGORY_MODULE_EXTERN(ELECTRAPLAYERRUNTIME_API, ElectraPlayer);

/** UE4 Profiler */
DECLARE_STATS_GROUP(TEXT("ElectraPlayer"), STATGROUP_ElectraPlayer, STATCAT_Advanced);


// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Stats/Stats.h"
#include "Stats/StatsMisc.h"

DECLARE_LOG_CATEGORY_EXTERN(TimingProfiler, Log, All);

DECLARE_STATS_GROUP(TEXT("TimingProfiler"), STATGROUP_TimingProfiler, STATCAT_Advanced);

/** Time spent on Frame Track drawing. */
DECLARE_CYCLE_STAT_EXTERN(TEXT("FrameTrackOnPaint"), STAT_FT_OnPaint, STATGROUP_TimingProfiler, );

/** Time spent on Graph Track drawing. */
DECLARE_CYCLE_STAT_EXTERN(TEXT("GraphTrackOnPaint"), STAT_GT_OnPaint, STATGROUP_TimingProfiler, );

/** Time spent on Timing Track drawing. */
DECLARE_CYCLE_STAT_EXTERN(TEXT("TimingTrackOnPaint"), STAT_TT_OnPaint, STATGROUP_TimingProfiler, );

/** Time spent on ticking profiler manager. */
DECLARE_CYCLE_STAT_EXTERN(TEXT("TimingProfilerTick"), STAT_TPM_Tick, STATGROUP_TimingProfiler, );

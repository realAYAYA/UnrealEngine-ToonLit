// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stats/StatsMisc.h"
#include "Stats/Stats.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"


#if !STATS && !UE_BUILD_DEBUG && defined(USE_LIGHTWEIGHT_STATS_FOR_HITCH_DETECTION) && USE_LIGHTWEIGHT_STATS_FOR_HITCH_DETECTION && USE_HITCH_DETECTION
#include "HAL/ThreadHeartBeat.h"
#include "HAL/ThreadManager.h"

void FLightweightStatScope::ReportHitch()
{
	if (StatString)
	{
		float Delta = float(FGameThreadHitchHeartBeat::Get().GetCurrentTime() - FGameThreadHitchHeartBeat::Get().GetFrameStartTime()) * 1000.0f;

		const uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
		const bool isGT = CurrentThreadId == GGameThreadId;
		const FString& ThreadString = FThreadManager::GetThreadName(CurrentThreadId);
		FString StackString = StatString; // possibly convert from ANSICHAR

		if (!isGT && (StackString == TEXT("STAT_EventWait") || StackString == TEXT("STAT_FQueuedThread_Run_WaitForWork")))
		{
			return;
		}

		UE_LOG(LogCore, Error, TEXT("Leaving stat scope on hitch (+%8.2fms) [%s] %s"), Delta, *ThreadString, *StackString);
	}
}

#endif


FConditionalScopeLogTime::FConditionalScopeLogTime( bool bCondition, const WIDECHAR* InName, FTotalTimeAndCount* InCumulative /*= nullptr */, EScopeLogTimeUnits InUnits /*= ScopeLog_Milliseconds */ )
: StartTime( bCondition ? FPlatformTime::Seconds() : 0.0 )
, Name( InName )
, Cumulative( InCumulative )
, Units( bCondition ? InUnits : ScopeLog_DontLog )
{}

FConditionalScopeLogTime::FConditionalScopeLogTime( bool bCondition, const ANSICHAR* InName, FTotalTimeAndCount* InCumulative /*= nullptr*/, EScopeLogTimeUnits InUnits /*= ScopeLog_Milliseconds */ )
: StartTime( bCondition ? FPlatformTime::Seconds() : 0.0 )
, Name( InName )
, Cumulative( InCumulative )
, Units( bCondition ? InUnits : ScopeLog_DontLog )
{}

FConditionalScopeLogTime::~FConditionalScopeLogTime()
{
	if (Units != ScopeLog_DontLog)
	{
		const double ScopedTime = FPlatformTime::Seconds() - StartTime;
		const FString DisplayUnitsString = GetDisplayUnitsString();
		if( Cumulative )
		{
			Cumulative->Key += ScopedTime;
			Cumulative->Value++;

			const double Average = Cumulative->Key / (double)Cumulative->Value;
			UE_LOG( LogStats, Log, TEXT( "%32s - %6.3f %s - Total %6.2f s / %5u / %6.3f %s" ), *Name, GetDisplayScopedTime(ScopedTime), *DisplayUnitsString, Cumulative->Key, Cumulative->Value, GetDisplayScopedTime(Average), *DisplayUnitsString );
		}
		else
		{
			UE_LOG( LogStats, Log, TEXT( "%32s - %6.3f %s" ), *Name, GetDisplayScopedTime(ScopedTime), *DisplayUnitsString );
		}
	}
}

double FConditionalScopeLogTime::GetDisplayScopedTime(double InScopedTime) const
{
	if (Units == ScopeLog_Seconds)
	{
		return InScopedTime;
	}

	return InScopedTime * 1000.0f;
}

FString FConditionalScopeLogTime::GetDisplayUnitsString() const
{
	if (Units == ScopeLog_Seconds)
	{
		return TEXT("s");
	}

	return TEXT("ms");
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/ExternalProfiler.h"
#include "Algo/Find.h"
#include "Logging/LogMacros.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Features/IModularFeatures.h"


#if UE_EXTERNAL_PROFILING_ENABLED

DEFINE_LOG_CATEGORY_STATIC( LogExternalProfiler, Log, All );


FExternalProfiler::FExternalProfiler()
	:	TimerCount( 0 ),
		// No way to tell whether we're paused or not so we assume paused as it makes the most sense
		bIsPaused( true )
{
}


void FExternalProfiler::PauseProfiler()
{
	ProfilerPauseFunction();
	bIsPaused = true;
}


void FExternalProfiler::ResumeProfiler()
{
	ProfilerResumeFunction();
	bIsPaused = false;
}


FName FExternalProfiler::GetFeatureName()
{
	static FName ProfilerFeatureName( "ExternalProfiler" );
	return ProfilerFeatureName;
}


bool FActiveExternalProfilerBase::bDidInitialize = false;

FExternalProfiler* FActiveExternalProfilerBase::ActiveProfiler = nullptr;

FExternalProfiler* FActiveExternalProfilerBase::InitActiveProfiler()
{
	// Create profiler on demand.
	if (ActiveProfiler == nullptr && !bDidInitialize && FCommandLine::IsInitialized())
	{
		const FName FeatureName = FExternalProfiler::GetFeatureName();
		TArray<FExternalProfiler*> AvailableProfilers = IModularFeatures::Get().GetModularFeatureImplementations<FExternalProfiler>(FeatureName);

		for (FExternalProfiler* CurProfiler : AvailableProfilers)
		{
			check(CurProfiler != nullptr);

#if 0
			// Logging disabled here as it can cause a stack overflow whilst flushing logs during EnginePreInit
			UE_LOG(LogExternalProfiler, Log, TEXT("Found external profiler: %s"), CurProfiler->GetProfilerName());
#endif

			// Check to see if the profiler was specified on the command-line (e.g., "-VTune")
			if (FParse::Param(FCommandLine::Get(), CurProfiler->GetProfilerName()))
			{
				ActiveProfiler = CurProfiler;
			}
		}

#if 0
		// Logging disabled here as it can cause a stack overflow whilst flushing logs during EnginePreInit
		if (ActiveProfiler != nullptr)
		{
			UE_LOG(LogExternalProfiler, Log, TEXT("Using external profiler: %s"), ActiveProfiler->GetProfilerName());
		}
		else
		{
			UE_LOG(LogExternalProfiler, Log, TEXT("No external profilers were discovered.  External profiling features will not be available."));
		}
#endif
		
		if (ActiveProfiler == nullptr)
		{
			FString ProfilerName = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_EXTERNAL_PROFILER"));
			if (!ProfilerName.IsEmpty())
			{
				FExternalProfiler** Found = Algo::FindByPredicate(AvailableProfilers,
					 [&ProfilerName](FExternalProfiler* Profiler) { return ProfilerName == Profiler->GetProfilerName();});
				ActiveProfiler = Found ? *Found : nullptr;
			}
		}

		// Don't try to initialize again this session
		bDidInitialize = true;
	}

	return ActiveProfiler;
}


void FScopedExternalProfilerBase::StartScopedTimer( const bool bWantPause )
{
	FExternalProfiler* Profiler = GetActiveProfiler();	

	if (Profiler != nullptr)
	{
		// Store the current state of profiler
		bWasPaused = Profiler->bIsPaused;

		// If the current profiler state isn't set to what we need, or if the global profiler sampler isn't currently
		// running, then start it now
		if (Profiler->TimerCount == 0 || bWantPause != Profiler->bIsPaused)
		{
			if( bWantPause )
			{
				Profiler->PauseProfiler();
			}
			else
			{
				Profiler->ResumeProfiler();
			}
		}

		// Increment number of overlapping timers
		Profiler->TimerCount++;
	}
}


void FScopedExternalProfilerBase::StopScopedTimer()
{
	FExternalProfiler* Profiler = GetActiveProfiler();

	if (Profiler != nullptr)
	{
		// Make sure a timer was already started
		if (Profiler->TimerCount > 0)
		{
			// Decrement timer count
			Profiler->TimerCount--;

			// Restore the previous state of VTune
			if (bWasPaused != Profiler->bIsPaused)
			{
				if (bWasPaused)
				{
					Profiler->PauseProfiler();
				}
				else
				{
					Profiler->ResumeProfiler();
				}
			}
		}
	}
}

#endif	// UE_EXTERNAL_PROFILING_ENABLED

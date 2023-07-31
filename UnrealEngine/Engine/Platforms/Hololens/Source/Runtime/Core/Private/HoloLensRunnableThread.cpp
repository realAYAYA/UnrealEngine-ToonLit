// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoloLensRunnableThread.h"
#include "Misc/OutputDeviceError.h"
#include "HAL/ExceptionHandling.h"
#include "Misc/FeedbackContext.h"
#include "CoreGlobals.h"
#include "HoloLensSystemIncludes.h"
#include <excpt.h>

DEFINE_LOG_CATEGORY_STATIC(LogThreadingWindows, Log, All);

extern CORE_API int32 ReportCrash(Windows::LPEXCEPTION_POINTERS ExceptionInfo);



uint32 FRunnableThreadHoloLens::GuardedRun()
{
	uint32 ExitCode = 1;

	FPlatformProcess::SetThreadAffinityMask(ThreadAffinityMask);

#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
	if (!FPlatformMisc::IsDebuggerPresent() || GAlwaysReportCrash)
	{
		__try
		{
			ExitCode = Run();
		}
		__except (ReportCrash(GetExceptionInformation()))
		{
			// Make sure the information which thread crashed makes it into the log.
			UE_LOG(LogThreadingWindows, Error, TEXT("Runnable thread %s crashed."), *ThreadName);
			GWarn->Flush();

			// Append the thread name at the end of the error report.
			FCString::Strncat(GErrorHist, LINE_TERMINATOR TEXT("Crash in runnable thread "), UE_ARRAY_COUNT(GErrorHist));
			FCString::Strncat(GErrorHist, *ThreadName, UE_ARRAY_COUNT(GErrorHist));

			ExitCode = 1;
			// Generate status report.				
			GError->HandleError();
			// Throw an error so that main thread shuts down too (otherwise task graph stalls forever).
			UE_LOG(LogThreadingWindows, Fatal, TEXT("Runnable thread %s crashed."), *ThreadName);
		}
	}
	else
#endif
	{
		ExitCode = Run();
	}

	return ExitCode;
}


uint32 FRunnableThreadHoloLens::Run()
{
	// Assume we'll fail init
	uint32 ExitCode = 1;
	check(Runnable);

	// Initialize the runnable object
	if (Runnable->Init() == true)
	{
		// Initialization has completed, release the sync event
		ThreadInitSyncEvent->Trigger();

		SetTls();

		// Now run the task that needs to be done
		ExitCode = Runnable->Run();
		// Allow any allocated resources to be cleaned up
		Runnable->Exit();
	}
	else
	{
		// Initialization has failed, release the sync event
		ThreadInitSyncEvent->Trigger();
	}

	return ExitCode;
}

int FRunnableThreadHoloLens::TranslateThreadPriority(EThreadPriority Priority)
{
	// If this triggers, 
	static_assert(TPri_Num == 7, "Need to add a case for new TPri_xxx enum value");

	switch (Priority)
	{
	case TPri_AboveNormal:			return THREAD_PRIORITY_ABOVE_NORMAL;
	case TPri_Normal:				return THREAD_PRIORITY_NORMAL;
	case TPri_BelowNormal:			return THREAD_PRIORITY_BELOW_NORMAL;
	case TPri_Highest:				return THREAD_PRIORITY_HIGHEST;
	case TPri_TimeCritical:			return THREAD_PRIORITY_HIGHEST;
	case TPri_Lowest:				return THREAD_PRIORITY_LOWEST;

		// There is no such things as slightly below normal on Windows.
		// This can't be below normal since we don't want latency sensitive task to go to efficient cores on Alder Lake.
	case TPri_SlightlyBelowNormal:	return THREAD_PRIORITY_NORMAL;

	default: UE_LOG(LogHAL, Fatal, TEXT("Unknown Priority passed to TranslateThreadPriority()")); return THREAD_PRIORITY_NORMAL;
	}
}

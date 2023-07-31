// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoloLensErrorOutputDevice.h"

#include "HoloLensApplication.h"
#include "HoloLensPlatformApplicationMisc.h"
#include "HAL/PlatformMisc.h"
#include "Microsoft/WindowsHWrapper.h"
#include "CoreGlobals.h"
#include "Misc/CoreDelegates.h"
#include "Misc/OutputDeviceRedirector.h"


FHoloLensErrorOutputDevice::FHoloLensErrorOutputDevice()
{}

void FHoloLensErrorOutputDevice::Serialize(const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	UE_DEBUG_BREAK();

	if (!GIsCriticalError)
	{
		int32 LastError = ::GetLastError();

		GIsCriticalError = 1;
		TCHAR ErrorBuffer[1024];
		ErrorBuffer[0] = 0;

		if (LastError == 0)
		{
			UE_LOG(LogHoloLens, Log, TEXT("HoloLens GetLastError: %s (%i)"), FPlatformMisc::GetSystemErrorMessage(ErrorBuffer, 1024, LastError), LastError);
		}
		else
		{
			UE_LOG(LogHoloLens, Error, TEXT("HoloLens GetLastError: %s (%i)"), FPlatformMisc::GetSystemErrorMessage(ErrorBuffer, 1024, LastError), LastError);
		}
	}
	else
	{
		UE_LOG(LogHoloLens, Error, TEXT("Error reentered: %s"), Msg);
	}

	if (GIsGuarded)
	{
		// Propagate error so structured exception handler can perform necessary work.
#if PLATFORM_EXCEPTIONS_DISABLED
		UE_DEBUG_BREAK();
#endif
		FPlatformMisc::RaiseException(1);
	}
	else
	{
		// We crashed outside the guarded code (e.g. appExit).
		HandleError();
		FPlatformMisc::RequestExit(true);
	}
}

void FHoloLensErrorOutputDevice::HandleError()
{
	// make sure we don't report errors twice
	static int32 CallCount = 0;
	int32 NewCallCount = FPlatformAtomics::InterlockedIncrement(&CallCount);
	if (NewCallCount != 1)
	{
		UE_LOG(LogHoloLens, Error, TEXT("HandleError re-entered."));
		return;
	}

	GIsGuarded = 0;
	GIsRunning = 0;
	GIsCriticalError = 1;
	GLogConsole = NULL;
	GErrorHist[UE_ARRAY_COUNT(GErrorHist) - 1] = 0;

	// Trigger the OnSystemFailure hook if it exists
	// make sure it happens after GIsGuarded is set to 0 in case this hook crashes
	FCoreDelegates::OnHandleSystemError.Broadcast();

	// Dump the error and flush the log.
#if !NO_LOGGING
	FDebug::LogFormattedMessageWithCallstack(LogHoloLens.GetCategoryName(), __FILE__, __LINE__, TEXT("=== Critical error: ==="), GErrorHist, ELogVerbosity::Error);
#endif
	GLog->Panic();

	// Copy to clipboard in non-cooked editor builds.
	FPlatformApplicationMisc::ClipboardCopy(GErrorHist);

	FPlatformMisc::SubmitErrorReport(GErrorHist, EErrorReportMode::Interactive);

	FCoreDelegates::OnShutdownAfterError.Broadcast();
}
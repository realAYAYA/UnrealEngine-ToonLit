// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Editor/EditorPerformanceSettings.h"
#include "Misc/CoreDelegates.h"
#include "LiveLinkHub.h"
#include "Misc/App.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/UObjectGlobals.h"

#if PLATFORM_MAC
#include "HAL/PlatformApplicationMisc.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLiveLinkHubApplication, Log, All);

void LiveLinkHubLoop(const TSharedPtr<FLiveLinkHub>& LiveLinkHub)
{
	// Disable throttling for the hub
	GetMutableDefault<UEditorPerformanceSettings>()->bThrottleCPUWhenNotForeground = false;

	{
		UE_LOG(LogLiveLinkHubApplication, Display, TEXT("LiveLinkHub Initialized (Version: %d.%d)"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION);

		double LastTime = FPlatformTime::Seconds();

		constexpr double IdealFrameRate = 60.0;
		constexpr float IdealFrameTime = 1.0f / IdealFrameRate;

		while (!IsEngineExitRequested())
		{
			const double CurrentTime = FPlatformTime::Seconds();
			const double DeltaTime = CurrentTime - LastTime;

			FApp::SetDeltaTime(DeltaTime);
			GEngine->UpdateTimeAndHandleMaxTickRate();

			CommandletHelpers::TickEngine(nullptr, DeltaTime);

			// This is normally ticked by OnSamplingInput.
			LiveLinkHub->Tick();

			// Run garbage collection for the UObjects for the rest of the frame or at least to 2 ms
			IncrementalPurgeGarbage(true, FMath::Max<float>(0.002f, IdealFrameTime - (FPlatformTime::Seconds() - LastTime)));

#if PLATFORM_MAC
			// Pumps the message from the main loop. (On Mac, we have a full application to get a proper console window to output logs)
			FPlatformApplicationMisc::PumpMessages(true);
#endif

			FCoreDelegates::OnEndFrame.Broadcast();

			// Throttle main thread main fps by sleeping if we still have time
			FPlatformProcess::Sleep(FMath::Max<float>(0.0f, IdealFrameTime - (FPlatformTime::Seconds() - LastTime)));

			LastTime = CurrentTime;
		}
	}
}

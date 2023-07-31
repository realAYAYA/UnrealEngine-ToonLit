// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterAppExit.h"
#include "Misc/DisplayClusterLog.h"
#include "Engine/GameEngine.h"

#include "Async/Async.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Public/UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#endif


void FDisplayClusterAppExit::ExitApplication(const FString& Msg, EExitType ExitType)
{
	if (GEngine && GEngine->IsEditor())
	{
#if WITH_EDITOR
		UE_LOG(LogDisplayClusterModule, Log, TEXT("PIE end requested - %s"), *Msg);
		GUnrealEd->RequestEndPlayMap();
#endif
	}
	else
	{
		if (ExitType == EExitType::Normal)
		{
			if (!IsEngineExitRequested())
			{
				UE_LOG(LogDisplayClusterModule, Log, TEXT("Exit requested - %s"), *Msg);

				if (IsInGameThread())
				{
					FPlatformMisc::RequestExit(false);
				}
				else
				{
					// For some reason UE4 generates crash info if FPlatformMisc::RequestExit gets called
					// from a thread other than GameThread. Since it may be called from the networking
					// session threads (failover pipeline), we don't want to generate unnecessary crash reports.
					AsyncTask(ENamedThreads::GameThread, []()
						{
							FPlatformMisc::RequestExit(false);
						});
				}
			}
		}
		else if(ExitType == EExitType::KillImmediately)
		{
			FProcHandle hProc = FPlatformProcess::OpenProcess(FPlatformProcess::GetCurrentProcessId());
			FPlatformProcess::TerminateProc(hProc, true);
		}
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Launcher/Launcher.h"

#include "HAL/RunnableThread.h"
#include "Launcher/LauncherWorker.h"
#include "Profiles/LauncherProfile.h"


/* Static class member instantiations
*****************************************************************************/

FThreadSafeCounter FLauncher::WorkerCounter;


/* ILauncher overrides
 *****************************************************************************/

ILauncherWorkerPtr FLauncher::Launch(const TSharedRef<ITargetDeviceProxyManager>& DeviceProxyManager, const ILauncherProfileRef& Profile)
{
	if (Profile->IsValidForLaunch())
	{
		FLauncherWorker* LauncherWorker = new FLauncherWorker(DeviceProxyManager, Profile);
		FString WorkerName(FString::Printf(TEXT("LauncherWorker%i"), WorkerCounter.Increment()));

		if ((LauncherWorker != nullptr) && (FRunnableThread::Create(LauncherWorker, *WorkerName) != nullptr))
		{
			ILauncherWorkerPtr Worker = MakeShareable(LauncherWorker);
			FLauncherWorkerStartedDelegate.Broadcast(Worker, Profile);
			return Worker;
		}
	}
	else
	{
		UE_LOG(LogLauncherProfile, Error, TEXT("Launcher profile '%s' for is not valid for launch."),
			*Profile->GetName());
		for (int32 I = 0; I < (int32)ELauncherProfileValidationErrors::Count; ++I)
		{
			ELauncherProfileValidationErrors::Type Error = (ELauncherProfileValidationErrors::Type)I;
			if (Profile->HasValidationError(Error))
			{
				UE_LOG(LogLauncherProfile, Error, TEXT("ValidationError: %s"), *LexToStringLocalized(Error));
			}
		}
	}

	return nullptr;
}

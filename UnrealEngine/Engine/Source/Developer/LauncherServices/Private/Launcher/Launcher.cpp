// Copyright Epic Games, Inc. All Rights Reserved.

#include "Launcher/Launcher.h"

#include "HAL/RunnableThread.h"
#include "Launcher/LauncherWorker.h"


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

	return nullptr;
}

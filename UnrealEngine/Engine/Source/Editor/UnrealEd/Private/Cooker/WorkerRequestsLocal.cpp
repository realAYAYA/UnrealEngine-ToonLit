// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorkerRequestsLocal.h"

#include "CookTypes.h"
#include "Cooker/CookRequests.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"

class FConfigFile;
class ITargetPlatform;

namespace UE::Cook
{
struct FInstigator;
struct FPackageData;

bool FWorkerRequestsLocal::HasExternalRequests() const
{
	return ExternalRequests.HasRequests();
}

int32 FWorkerRequestsLocal::GetNumExternalRequests() const
{
	return ExternalRequests.GetNumRequests();
}

EExternalRequestType FWorkerRequestsLocal::DequeueNextCluster(TArray<FSchedulerCallback>& OutCallbacks, TArray<FFilePlatformRequest>& OutBuildRequests)
{
	return ExternalRequests.DequeueNextCluster(OutCallbacks, OutBuildRequests);
}

bool FWorkerRequestsLocal::DequeueSchedulerCallbacks(TArray<FSchedulerCallback>& OutCallbacks)
{
	return ExternalRequests.DequeueCallbacks(OutCallbacks);
}

void FWorkerRequestsLocal::DequeueAllExternal(TArray<FSchedulerCallback>& OutCallbacks, TArray<FFilePlatformRequest>& OutCookRequests)
{
	ExternalRequests.DequeueAll(OutCallbacks, OutCookRequests);
}

void FWorkerRequestsLocal::QueueDiscoveredPackage(UCookOnTheFlyServer& COTFS, FPackageData& PackageData,
	FInstigator&& Instigator, FDiscoveredPlatformSet&& ReachablePlatforms, bool bUrgent)
{
	COTFS.QueueDiscoveredPackageOnDirector(PackageData, MoveTemp(Instigator), MoveTemp(ReachablePlatforms), bUrgent);
}

void FWorkerRequestsLocal::AddStartCookByTheBookRequest(FFilePlatformRequest&& Request)
{
	ExternalRequests.EnqueueUnique(MoveTemp(Request));
}

void FWorkerRequestsLocal::InitializeCookOnTheFly()
{
	ExternalRequests.CookRequestEvent.Reset(FPlatformProcess::GetSynchEventFromPool());
}

void FWorkerRequestsLocal::AddCookOnTheFlyRequest(FFilePlatformRequest&& Request)
{
	ExternalRequests.EnqueueUnique(MoveTemp(Request), true /* bForceFrontOfQueue */);
	if (ExternalRequests.CookRequestEvent)
	{
		ExternalRequests.CookRequestEvent->Trigger();
	}
}

void FWorkerRequestsLocal::AddCookOnTheFlyCallback(FSchedulerCallback&& Callback)
{
	ExternalRequests.AddCallback(MoveTemp(Callback));
	if (ExternalRequests.CookRequestEvent)
	{
		ExternalRequests.CookRequestEvent->Trigger();
	}
}

void FWorkerRequestsLocal::WaitForCookOnTheFlyEvents(int TimeoutMs)
{
	if (ExternalRequests.CookRequestEvent)
	{
		ExternalRequests.CookRequestEvent->Wait(TimeoutMs, true /* bIgnoreThreadIdleStats */);
	}
}

void FWorkerRequestsLocal::AddEditorActionCallback(FSchedulerCallback&& Callback)
{
	ExternalRequests.AddCallback(MoveTemp(Callback));
}

void FWorkerRequestsLocal::AddPublicInterfaceRequest(FFilePlatformRequest&& Request, bool bForceFrontOfQueue)
{
	ExternalRequests.EnqueueUnique(MoveTemp(Request), bForceFrontOfQueue);
}

void FWorkerRequestsLocal::RemapTargetPlatforms(const TMap<ITargetPlatform*, ITargetPlatform*>& Remap)
{
	ExternalRequests.RemapTargetPlatforms(Remap);
}

void FWorkerRequestsLocal::OnRemoveSessionPlatform(const ITargetPlatform* TargetPlatform)
{
	ExternalRequests.OnRemoveSessionPlatform(TargetPlatform);
}

void FWorkerRequestsLocal::ReportDemoteToIdle(UE::Cook::FPackageData& PackageData, ESuppressCookReason Reason)
{
}

void FWorkerRequestsLocal::ReportPromoteToSaveComplete(UE::Cook::FPackageData& PackageData)
{
}

void FWorkerRequestsLocal::GetInitializeConfigSettings(UCookOnTheFlyServer& COTFS,
	const FString& OutputDirectoryOverride, UE::Cook::FInitializeConfigSettings& Settings)
{
	Settings.LoadLocal(OutputDirectoryOverride);
}

void FWorkerRequestsLocal::GetBeginCookConfigSettings(UCookOnTheFlyServer& COTFS, FBeginCookContext& BeginContext, UE::Cook::FBeginCookConfigSettings& Settings)
{
	Settings.LoadLocal(BeginContext);
}

void FWorkerRequestsLocal::GetBeginCookIterativeFlags(UCookOnTheFlyServer& COTFS, FBeginCookContext& BeginContext)
{
	BeginContext.COTFS.LoadBeginCookIterativeFlagsLocal(BeginContext);
}

ECookMode::Type FWorkerRequestsLocal::GetDirectorCookMode(UCookOnTheFlyServer& COTFS)
{
	return COTFS.GetCookMode();
}

void FWorkerRequestsLocal::LogAllRequestedFiles()
{
	ExternalRequests.LogAllRequestedFiles();
}

}
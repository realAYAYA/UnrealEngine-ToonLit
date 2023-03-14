// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Synchronization/DisplayClusterRenderSyncPolicyBase.h"
#include "Render/Synchronization/DisplayClusterRenderSyncHelper.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Cluster/Controller/IDisplayClusterClusterNodeController.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

#include "RHIResources.h"


static TAutoConsoleVariable<int32> CVarSyncDiagnosticsVBlankMonitoring(
	TEXT("nDisplay.sync.diag.VBlankMonitoring"),
	0,
	TEXT("Sync diagnostics: V-blank monitoring\n")
	TEXT("0 : disabled\n")
	TEXT("1 : enabled (if policy supports only)\n")
	,
	ECVF_ReadOnly
);


bool FDisplayClusterRenderSyncPolicyBase::Initialize()
{
	// In case v-blank monitoring was requested and this feature is supported by
	// the sync policy implementation, start v-blank monitoring.
	if (!!CVarSyncDiagnosticsVBlankMonitoring.GetValueOnGameThread() && IsWaitForVBlankFeatureSupported())
	{
		VBlankMonitor = MakeUnique<FDisplayClusterVBlankMonitor>(*this);
	}

	return true;
}

void FDisplayClusterRenderSyncPolicyBase::SyncBarrierRenderThread()
{
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(nDisplay SyncPolicyBase::SyncBarrier);
		GDisplayCluster->GetPrivateClusterMgr()->GetClusterNodeController()->WaitForSwapSync();
	}
}

void FDisplayClusterRenderSyncPolicyBase::WaitForFrameCompletion()
{
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		FRHIViewport* const Viewport = GEngine->GameViewport->Viewport->GetViewportRHI().GetReference();
		check(Viewport);

		Viewport->IssueFrameEvent();
		Viewport->WaitForFrameEventCompletion();
	}
}

bool FDisplayClusterRenderSyncPolicyBase::IsWaitForVBlankFeatureSupported()
{
	return FDisplayClusterRenderSyncHelper::Get().IsWaitForVBlankSupported();
}

bool FDisplayClusterRenderSyncPolicyBase::WaitForVBlank()
{
	return FDisplayClusterRenderSyncHelper::Get().WaitForVBlank();
}

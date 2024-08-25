// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Synchronization/DisplayClusterRenderSyncPolicyBase.h"
#include "Components/Viewport.h"
#include "Render/Synchronization/DisplayClusterRenderSyncHelper.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Cluster/Controller/IDisplayClusterClusterNodeController.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

#include "RHI.h"
#include "RHIResources.h"


bool FDisplayClusterRenderSyncPolicyBase::Initialize()
{
	return true;
}

void FDisplayClusterRenderSyncPolicyBase::SyncOnBarrier()
{
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(nDisplay SyncOnBarrier);
		GDisplayCluster->GetPrivateClusterMgr()->GetClusterNodeController()->SyncOnBarrier();
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

bool FDisplayClusterRenderSyncPolicyBase::GetMaximumFrameLatency(uint32& OutMaximumFrameLatency)
{
	return FDisplayClusterRenderSyncHelper::Get().GetMaximumFrameLatency(OutMaximumFrameLatency);
}

bool FDisplayClusterRenderSyncPolicyBase::SetMaximumFrameLatency(uint32 MaximumFrameLatency)
{
	return FDisplayClusterRenderSyncHelper::Get().SetMaximumFrameLatency(MaximumFrameLatency);
}

bool FDisplayClusterRenderSyncPolicyBase::IsWaitForVBlankFeatureSupported()
{
	return FDisplayClusterRenderSyncHelper::Get().IsWaitForVBlankSupported();
}

bool FDisplayClusterRenderSyncPolicyBase::WaitForVBlank()
{
	return FDisplayClusterRenderSyncHelper::Get().WaitForVBlank();
}

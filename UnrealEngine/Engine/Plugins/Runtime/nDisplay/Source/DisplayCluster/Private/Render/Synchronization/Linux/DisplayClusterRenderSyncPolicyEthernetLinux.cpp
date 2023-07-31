// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Synchronization/DisplayClusterRenderSyncPolicyEthernet.h"


bool FDisplayClusterRenderSyncPolicyEthernet::SynchronizeClusterRendering(int32& InOutSyncInterval)
{
	// Wait unless the frame is rendered
	WaitForFrameCompletion();
	// Sync on the network barrier
	SyncBarrierRenderThread();
	// Ask engine to present the frame
	return true;
}

void FDisplayClusterRenderSyncPolicyEthernet::Procedure_SynchronizePresent()
{
}

void FDisplayClusterRenderSyncPolicyEthernet::Step_InitializeFrameSynchronization()
{
}

void FDisplayClusterRenderSyncPolicyEthernet::Step_WaitForFrameCompletion()
{
}

void FDisplayClusterRenderSyncPolicyEthernet::Step_WaitForEthernetBarrierSignal_1()
{
}

void FDisplayClusterRenderSyncPolicyEthernet::Step_SkipPresentationOnClosestVBlank()
{
}

void FDisplayClusterRenderSyncPolicyEthernet::Step_WaitForEthernetBarrierSignal_2()
{
}

void FDisplayClusterRenderSyncPolicyEthernet::Step_Present()
{
}

void FDisplayClusterRenderSyncPolicyEthernet::Step_FinalizeFrameSynchronization()
{
}

double FDisplayClusterRenderSyncPolicyEthernet::GetVBlankTimestamp()
{
	return 0.l;
}

double FDisplayClusterRenderSyncPolicyEthernet::GetRefreshPeriod()
{
	return 0.l;
}

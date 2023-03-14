// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Synchronization/DisplayClusterRenderSyncPolicyEthernetBarrier.h"
#include "DisplayClusterConfigurationStrings.h"


FName FDisplayClusterRenderSyncPolicyEthernetBarrier::GetName() const
{
	static const FName Policy = DisplayClusterConfigurationStrings::config::cluster::render_sync::EthernetBarrier;
	return Policy;
}

bool FDisplayClusterRenderSyncPolicyEthernetBarrier::SynchronizeClusterRendering(int32& InOutSyncInterval)
{
	// Wait unless the frame is rendered
	WaitForFrameCompletion();
	// Sync on the network barrier
	SyncBarrierRenderThread();
	// Ask engine to present the frame
	return true;
}

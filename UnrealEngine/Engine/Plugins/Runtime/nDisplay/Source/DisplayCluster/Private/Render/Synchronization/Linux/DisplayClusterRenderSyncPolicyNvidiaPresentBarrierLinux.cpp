// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Synchronization/DisplayClusterRenderSyncPolicyNvidiaPresentBarrier.h"


FDisplayClusterRenderSyncPolicyNvidiaPresentBarrier::FDisplayClusterRenderSyncPolicyNvidiaPresentBarrier(const TMap<FString, FString>& Parameters)
	: Super(Parameters)
{
}

FDisplayClusterRenderSyncPolicyNvidiaPresentBarrier::~FDisplayClusterRenderSyncPolicyNvidiaPresentBarrier()
{
}

bool FDisplayClusterRenderSyncPolicyNvidiaPresentBarrier::InitializePresentBarrier()
{
	return true;
}

bool FDisplayClusterRenderSyncPolicyNvidiaPresentBarrier::SynchronizeClusterRendering(int32& InOutSyncInterval)
{
	// Wait unless the frame is rendered
	WaitForFrameCompletion();
	// Sync on the network barrier
	SyncOnBarrier();
	// Ask engine to present the frame
	return true;
}

void FDisplayClusterRenderSyncPolicyNvidiaPresentBarrier::OnFramePresented(bool bNativePresent)
{
}

void FDisplayClusterRenderSyncPolicyNvidiaPresentBarrier::LogPresentBarrierStats()
{
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Synchronization/DisplayClusterRenderSyncPolicyNvidiaSwapBarrier.h"


FDisplayClusterRenderSyncPolicyNvidiaSwapBarrier::FDisplayClusterRenderSyncPolicyNvidiaSwapBarrier(const TMap<FString, FString>& Parameters)
	: Super(Parameters)
{
}

FDisplayClusterRenderSyncPolicyNvidiaSwapBarrier::~FDisplayClusterRenderSyncPolicyNvidiaSwapBarrier()
{
}

bool FDisplayClusterRenderSyncPolicyNvidiaSwapBarrier::InitializeNvidiaSwapLock()
{
	return true;
}

bool FDisplayClusterRenderSyncPolicyNvidiaSwapBarrier::SynchronizeClusterRendering(int32& InOutSyncInterval)
{
	// Wait unless the frame is rendered
	WaitForFrameCompletion();
	// Sync on the network barrier
	SyncOnBarrier();
	// Ask engine to present the frame
	return true;
}

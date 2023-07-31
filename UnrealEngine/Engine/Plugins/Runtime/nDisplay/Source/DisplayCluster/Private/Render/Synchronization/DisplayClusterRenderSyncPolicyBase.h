// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Synchronization/IDisplayClusterRenderSyncPolicy.h"
#include "Render/Synchronization/DisplayClusterVblankMonitor.h"


/**
 * Base synchronization policy
 */
class FDisplayClusterRenderSyncPolicyBase
	: public IDisplayClusterRenderSyncPolicy
{
public:
	FDisplayClusterRenderSyncPolicyBase(const TMap<FString, FString>& InParameters)
		: Parameters(InParameters)
	{ }

	virtual ~FDisplayClusterRenderSyncPolicyBase() = default;

public:
	// Non-constructor initialization
	virtual bool Initialize() override;

	// VBlank related functions
	virtual bool IsWaitForVBlankFeatureSupported() override;
	virtual bool WaitForVBlank() override;

public:
	// Performs Ethernet barrier syncrhonization with other nodes in the cluster
	void SyncBarrierRenderThread();

	// Returns policy configuration parameters
	const TMap<FString, FString>& GetParameters() const
	{
		return Parameters;
	}

protected:
	// Wait for frame rendering completion using a GPU fence
	virtual void WaitForFrameCompletion();

private:
	// Sync policy configuration parameters
	TMap<FString, FString> Parameters;

	// V-blank monitoring thread. Performs V-blank polling on the top level of the sync policies hierarchy.
	TUniquePtr<FDisplayClusterVBlankMonitor> VBlankMonitor;
};

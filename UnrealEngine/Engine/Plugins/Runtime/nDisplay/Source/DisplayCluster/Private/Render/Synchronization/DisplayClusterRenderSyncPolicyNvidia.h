// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Synchronization/DisplayClusterRenderSyncPolicyBase.h"
#include "DisplayClusterConfigurationStrings.h"


/**
 * Base NVIDIA FrameLock & SwapSync synchronization policy
 */
class FDisplayClusterRenderSyncPolicyNvidia
	: public FDisplayClusterRenderSyncPolicyBase
{
public:

	using Super = FDisplayClusterRenderSyncPolicyBase;

	FDisplayClusterRenderSyncPolicyNvidia(const TMap<FString, FString>& Parameters);
	virtual ~FDisplayClusterRenderSyncPolicyNvidia();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterRenderSyncPolicy
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual FName GetName() const override
	{
		static const FName NvidiaPolicy = DisplayClusterConfigurationStrings::config::cluster::render_sync::Nvidia;
		return NvidiaPolicy;
	}

	// Initialization
	virtual bool Initialize() override;

	// Performs rendering and frame output synchronization
	virtual bool SynchronizeClusterRendering(int32& InOutSyncInterval) override;

private:
	bool InitializeNvidiaSwapLock();

private:
	bool bNvApiInitialized = false;
	bool bNvApiBarrierSet  = false;

	bool bNvDiagInit       = true;
	bool bNvDiagPresent    = true;
	bool bNvDiagWaitQueue  = false;
	bool bNvDiagCompletion = false;

	uint32 RequestedGroup   = 1;
	uint32 RequestedBarrier = 1;

	uint32 NvPresentBarrierCount      = 0;
	uint32 NvPresentBarrierCountLimit = 0;

	uint32 NvCompletionCount      = 0;
	uint32 NvCompletionCountLimit = 0;
};

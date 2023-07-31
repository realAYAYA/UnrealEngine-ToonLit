// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Device/DisplayClusterDeviceBase.h"

/**
 * Top-bottom passive stereoscopic device
 */
class FDisplayClusterDeviceTopBottomDX12
	: public FDisplayClusterDeviceBase
{
public:
	FDisplayClusterDeviceTopBottomDX12();
	virtual ~FDisplayClusterDeviceTopBottomDX12() = default;

protected:
	virtual FDisplayClusterPresentationBase* CreatePresentationObject(FViewport* const Viewport, TSharedPtr<IDisplayClusterRenderSyncPolicy>& SyncPolicy) override;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Device/DisplayClusterDeviceBase.h"

/**
 * Side-by-side passive stereoscopic device
 */
class FDisplayClusterDeviceSideBySideDX12
	: public FDisplayClusterDeviceBase
{
public:
	FDisplayClusterDeviceSideBySideDX12();
	virtual ~FDisplayClusterDeviceSideBySideDX12() = default;

protected:
	virtual FDisplayClusterPresentationBase* CreatePresentationObject(FViewport* const Viewport, TSharedPtr<IDisplayClusterRenderSyncPolicy>& SyncPolicy) override;
};

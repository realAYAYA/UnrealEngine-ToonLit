// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Device/DisplayClusterDeviceBase.h"

/**
 * Side-by-side passive stereoscopic device
 */
class FDisplayClusterDeviceSideBySideDX11
	: public FDisplayClusterDeviceBase
{
public:
	FDisplayClusterDeviceSideBySideDX11();
	virtual ~FDisplayClusterDeviceSideBySideDX11() = default;

protected:
	virtual FDisplayClusterPresentationBase* CreatePresentationObject(FViewport* const Viewport, TSharedPtr<IDisplayClusterRenderSyncPolicy>& SyncPolicy) override;
};

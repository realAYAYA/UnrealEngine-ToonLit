// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Device/DisplayClusterDeviceBase.h"

/**
 * Side-by-side passive stereoscopic device (Vulkan RHI)
 */
class FDisplayClusterDeviceSideBySideVulkan
	: public FDisplayClusterDeviceBase
{
public:
	FDisplayClusterDeviceSideBySideVulkan();
	virtual ~FDisplayClusterDeviceSideBySideVulkan() = default;

protected:
	virtual FDisplayClusterPresentationBase* CreatePresentationObject(FViewport* const Viewport, TSharedPtr<IDisplayClusterRenderSyncPolicy>& SyncPolicy) override;
};

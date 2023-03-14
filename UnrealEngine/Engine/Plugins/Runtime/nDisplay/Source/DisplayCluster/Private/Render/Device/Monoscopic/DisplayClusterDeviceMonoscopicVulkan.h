// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Device/DisplayClusterDeviceBase.h"

/**
 * Monoscopic render device (Vulkan)
 */
class FDisplayClusterDeviceMonoscopicVulkan
	: public FDisplayClusterDeviceBase
{
public:
	FDisplayClusterDeviceMonoscopicVulkan();
	virtual ~FDisplayClusterDeviceMonoscopicVulkan() = default;

protected:
	virtual FDisplayClusterPresentationBase* CreatePresentationObject(FViewport* const Viewport, TSharedPtr<IDisplayClusterRenderSyncPolicy>& SyncPolicy) override;
};

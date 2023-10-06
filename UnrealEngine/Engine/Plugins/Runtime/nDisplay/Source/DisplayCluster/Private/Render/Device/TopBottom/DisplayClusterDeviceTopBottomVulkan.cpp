// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Device/TopBottom/DisplayClusterDeviceTopBottomVulkan.h"
#include "Render/Presentation/DisplayClusterPresentationVulkan.h"


FDisplayClusterDeviceTopBottomVulkan::FDisplayClusterDeviceTopBottomVulkan()
	: FDisplayClusterDeviceBase(EDisplayClusterRenderFrameMode::TopBottom)
{
}

FDisplayClusterPresentationBase* FDisplayClusterDeviceTopBottomVulkan::CreatePresentationObject(FViewport* const Viewport, TSharedPtr<IDisplayClusterRenderSyncPolicy>& SyncPolicy)
{
	return new FDisplayClusterPresentationVulkan(Viewport, SyncPolicy);
}

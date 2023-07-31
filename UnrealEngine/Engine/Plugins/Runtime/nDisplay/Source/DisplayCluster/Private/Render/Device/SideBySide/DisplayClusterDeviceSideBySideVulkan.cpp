// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Device/SideBySide/DisplayClusterDeviceSideBySideVulkan.h"
#include "Render/Presentation/DisplayClusterPresentationVulkan.h"


FDisplayClusterDeviceSideBySideVulkan::FDisplayClusterDeviceSideBySideVulkan()
	: FDisplayClusterDeviceBase(EDisplayClusterRenderFrameMode::SideBySide)
{
}

FDisplayClusterPresentationBase* FDisplayClusterDeviceSideBySideVulkan::CreatePresentationObject(FViewport* const Viewport, TSharedPtr<IDisplayClusterRenderSyncPolicy>& SyncPolicy)
{
	return new FDisplayClusterPresentationVulkan(Viewport, SyncPolicy);
}

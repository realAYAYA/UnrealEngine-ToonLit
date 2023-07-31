// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Device/QuadBufferStereo/DisplayClusterDeviceQuadBufferStereoVulkan.h"
#include "Render/Presentation/DisplayClusterPresentationVulkan.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterDeviceQuadBufferStereoVulkan::FDisplayClusterDeviceQuadBufferStereoVulkan()
	: FDisplayClusterDeviceQuadBufferStereoBase()
{
}

FDisplayClusterPresentationBase* FDisplayClusterDeviceQuadBufferStereoVulkan::CreatePresentationObject(FViewport* const Viewport, TSharedPtr<IDisplayClusterRenderSyncPolicy>& SyncPolicy)
{
	return new FDisplayClusterPresentationVulkan(Viewport, SyncPolicy);
}

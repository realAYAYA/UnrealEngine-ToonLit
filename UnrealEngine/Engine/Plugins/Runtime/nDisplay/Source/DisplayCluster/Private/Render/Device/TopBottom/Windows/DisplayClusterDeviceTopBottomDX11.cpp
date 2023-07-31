// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Device/TopBottom/Windows/DisplayClusterDeviceTopBottomDX11.h"
#include "Render/Presentation/Windows/DisplayClusterPresentationDX11.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterDeviceTopBottomDX11::FDisplayClusterDeviceTopBottomDX11()
	: FDisplayClusterDeviceBase(EDisplayClusterRenderFrameMode::TopBottom)
{
}

FDisplayClusterPresentationBase* FDisplayClusterDeviceTopBottomDX11::CreatePresentationObject(FViewport* const Viewport, TSharedPtr<IDisplayClusterRenderSyncPolicy>& SyncPolicy)
{
	return new FDisplayClusterPresentationDX11(Viewport, SyncPolicy);
}

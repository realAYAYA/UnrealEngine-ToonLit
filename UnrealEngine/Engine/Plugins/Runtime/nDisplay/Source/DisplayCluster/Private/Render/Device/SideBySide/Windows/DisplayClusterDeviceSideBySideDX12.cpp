// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Device/SideBySide/Windows/DisplayClusterDeviceSideBySideDX12.h"
#include "Render/Presentation/Windows/DisplayClusterPresentationDX12.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterDeviceSideBySideDX12::FDisplayClusterDeviceSideBySideDX12()
	: FDisplayClusterDeviceBase(EDisplayClusterRenderFrameMode::SideBySide)
{
}

FDisplayClusterPresentationBase* FDisplayClusterDeviceSideBySideDX12::CreatePresentationObject(FViewport* const Viewport, TSharedPtr<IDisplayClusterRenderSyncPolicy>& SyncPolicy)
{
	return new FDisplayClusterPresentationDX12(Viewport, SyncPolicy);
}

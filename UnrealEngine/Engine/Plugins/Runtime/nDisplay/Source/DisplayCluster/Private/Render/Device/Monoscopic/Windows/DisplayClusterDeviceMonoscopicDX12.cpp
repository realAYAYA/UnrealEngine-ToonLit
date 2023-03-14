// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Device/Monoscopic/Windows/DisplayClusterDeviceMonoscopicDX12.h"
#include "Render/Presentation/Windows/DisplayClusterPresentationDX12.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterDeviceMonoscopicDX12::FDisplayClusterDeviceMonoscopicDX12()
	: FDisplayClusterDeviceBase(EDisplayClusterRenderFrameMode::Mono)
{
}

FDisplayClusterPresentationBase* FDisplayClusterDeviceMonoscopicDX12::CreatePresentationObject(FViewport* const Viewport, TSharedPtr<IDisplayClusterRenderSyncPolicy>& SyncPolicy)
{
	return new FDisplayClusterPresentationDX12(Viewport, SyncPolicy);
}

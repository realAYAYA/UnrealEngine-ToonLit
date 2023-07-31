// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Device/Monoscopic/Windows/DisplayClusterDeviceMonoscopicDX11.h"
#include "Render/Presentation/Windows/DisplayClusterPresentationDX11.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterDeviceMonoscopicDX11::FDisplayClusterDeviceMonoscopicDX11()
	: FDisplayClusterDeviceBase(EDisplayClusterRenderFrameMode::Mono)
{
}

FDisplayClusterPresentationBase* FDisplayClusterDeviceMonoscopicDX11::CreatePresentationObject(FViewport* const Viewport, TSharedPtr<IDisplayClusterRenderSyncPolicy>& SyncPolicy)
{
	return new FDisplayClusterPresentationDX11(Viewport, SyncPolicy);
}

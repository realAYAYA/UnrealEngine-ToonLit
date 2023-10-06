// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportManagerViewPointExtension.h"

#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewport.h"

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportManagerViewPointExtension
///////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterViewportManagerViewPointExtension::FDisplayClusterViewportManagerViewPointExtension(const FAutoRegister& AutoRegister, const FDisplayClusterViewportManager* InViewportManager)
	: FSceneViewExtensionBase(AutoRegister)
	, ViewportManagerWeakPtr(InViewportManager->AsShared())
{ }

FDisplayClusterViewportManagerViewPointExtension::~FDisplayClusterViewportManagerViewPointExtension()
{
	ViewportManagerWeakPtr.Reset();
}

bool FDisplayClusterViewportManagerViewPointExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return IsActive() && Context.IsStereoSupported();
}

void FDisplayClusterViewportManagerViewPointExtension::SetupViewPoint(APlayerController* Player, FMinimalViewInfo& InOutViewInfo)
{
	if (IDisplayClusterViewport* DCViewport = IsActive() ? GetViewportManager()->FindViewport(CurrentStereoViewIndex) : nullptr)
	{
		DCViewport->SetupViewPoint(InOutViewInfo);
	}
}

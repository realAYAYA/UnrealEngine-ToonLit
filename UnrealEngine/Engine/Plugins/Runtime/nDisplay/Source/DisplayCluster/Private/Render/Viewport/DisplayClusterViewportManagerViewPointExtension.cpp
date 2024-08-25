// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportManagerViewPointExtension.h"

#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewport.h"

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportManagerViewPointExtension
///////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterViewportManagerViewPointExtension::FDisplayClusterViewportManagerViewPointExtension(const FAutoRegister& AutoRegister, const TSharedRef<FDisplayClusterViewportConfiguration, ESPMode::ThreadSafe>& InConfiguration)
	: FSceneViewExtensionBase(AutoRegister)
	, Configuration(InConfiguration)
{ }

bool FDisplayClusterViewportManagerViewPointExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return IsActive() && Context.IsStereoSupported();
}

/** True, if VE can be used at the moment. */
bool FDisplayClusterViewportManagerViewPointExtension::IsActive() const
{
	return Configuration->GetViewportManager() != nullptr && CurrentStereoViewIndex != INDEX_NONE;
}

void FDisplayClusterViewportManagerViewPointExtension::SetupViewPoint(APlayerController* Player, FMinimalViewInfo& InOutViewInfo)
{
	if (IDisplayClusterViewport* DCViewport = IsActive() ? Configuration->GetViewportManager()->FindViewport(CurrentStereoViewIndex) : nullptr)
	{
		DCViewport->SetupViewPoint(InOutViewInfo);
	}
}

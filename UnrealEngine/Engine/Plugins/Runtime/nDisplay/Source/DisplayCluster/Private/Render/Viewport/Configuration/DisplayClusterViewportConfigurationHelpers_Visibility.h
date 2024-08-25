// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FDisplayClusterViewportConfiguration;
class FDisplayClusterViewport;
struct FDisplayClusterConfigurationICVFX_VisibilityList;

/**
* Visibility configuration helper class.
*/
class FDisplayClusterViewportConfigurationHelpers_Visibility
{
public:
	// Update ShowOnly list for DstViewport
	static void UpdateShowOnlyList(FDisplayClusterViewport& DstViewport, const FDisplayClusterConfigurationICVFX_VisibilityList& InVisibilityList);

	// Update hide lists for DstViewports (lightcards, chromakey, stage settings hide list, outer viewports hide list, etc)
	static void UpdateHideList_ICVFX(FDisplayClusterViewportConfiguration& InConfiguration, TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>>& DstViewports);
	
	// Append to exist hide list (must be call after UpdateHideList_ICVFX)
	static void AppendHideList_ICVFX(FDisplayClusterViewport& DstViewport, const FDisplayClusterConfigurationICVFX_VisibilityList& InHideList);
};


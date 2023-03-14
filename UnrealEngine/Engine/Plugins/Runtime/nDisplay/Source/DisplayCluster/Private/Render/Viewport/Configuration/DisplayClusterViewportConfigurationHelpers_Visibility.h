// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ADisplayClusterRootActor;
class FDisplayClusterViewport;

struct FDisplayClusterConfigurationICVFX_VisibilityList;

class FDisplayClusterViewportConfigurationHelpers_Visibility
{
public:
	// Retur true, if visibility list not empty
	static bool IsValid(const FDisplayClusterConfigurationICVFX_VisibilityList& InVisibilityList);

	// Update ShowOnly list for DstViewport
	static void UpdateShowOnlyList(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, const FDisplayClusterConfigurationICVFX_VisibilityList& InVisibilityList);

	// Update hide lists for DstViewports (lightcards, chromakey, stage settings hide list, outer viewports hide list, etc)
	static void UpdateHideList_ICVFX(TArray<FDisplayClusterViewport*>& DstViewports, ADisplayClusterRootActor& InRootActor);
	
	// Append to exist hide list (must be call after UpdateHideList_ICVFX)
	static void AppendHideList_ICVFX(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& InRootActor, const FDisplayClusterConfigurationICVFX_VisibilityList& InHideList);
};


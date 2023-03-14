// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SceneTypes.h"
#include "ActorLayerUtilities.h"

class FDisplayClusterViewport;
class FSceneView;
class UWorld;

enum class EDisplayClusterViewport_VisibilityMode : uint8
{
	None,
	ShowOnly,
	Hide
};

// GameThread-only data
class FDisplayClusterViewport_VisibilitySettings
{
public:
	virtual ~FDisplayClusterViewport_VisibilitySettings() = default;

public:
	// Reset actor layers visibility rules
	void ResetConfiguration()
	{
		LayersMode = EDisplayClusterViewport_VisibilityMode::None;

		ActorLayers.Empty();
		AdditionalComponentsList.Empty();
		RootActorHidePrimitivesList.Empty();
	}

	bool AppendHideList(const TArray<FName>& InActorLayers, const TSet<FPrimitiveComponentId>& InAdditionalComponentsList)
	{
		if (LayersMode == EDisplayClusterViewport_VisibilityMode::Hide)
		{
			ActorLayers.Append(InActorLayers);
			AdditionalComponentsList.Append(InAdditionalComponentsList);
			return true;
		}

		return false;
	}

	void UpdateConfiguration(EDisplayClusterViewport_VisibilityMode InMode, const TArray<FName>& InActorLayers, const TSet<FPrimitiveComponentId>& InAdditionalComponentsList)
	{

		LayersMode = InMode;
		ActorLayers = InActorLayers;
		AdditionalComponentsList = InAdditionalComponentsList;
	}

	void SetRootActorHideList(TSet<FPrimitiveComponentId>& InHidePrimitivesList)
	{
		RootActorHidePrimitivesList = InHidePrimitivesList;
	}

	void SetupSceneView(UWorld* World, FSceneView& InOutView) const;

private:
	EDisplayClusterViewport_VisibilityMode LayersMode = EDisplayClusterViewport_VisibilityMode::None;
	TArray<FName> ActorLayers;
	TSet<FPrimitiveComponentId> AdditionalComponentsList;

	// Additional hide primitives list from root actor
	TSet<FPrimitiveComponentId> RootActorHidePrimitivesList;
};


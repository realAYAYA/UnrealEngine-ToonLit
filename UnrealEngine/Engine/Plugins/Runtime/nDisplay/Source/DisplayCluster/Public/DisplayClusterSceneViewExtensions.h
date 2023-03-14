// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"

class IDisplayClusterViewportManager;

/** Contains information about the context in which this scene view extension will be used. */
struct FDisplayClusterSceneViewExtensionContext : public FSceneViewExtensionContext
{
private:
	//~ FSceneViewExtensionContext Interface
	virtual FName GetRTTI() const override { return TEXT("FDisplayClusterSceneViewExtensionContext"); }

	virtual bool IsHMDSupported() const override
	{
		// Disable all HMD extensions for nDisplay render
		return false;
	}

public:

	// The id of the nDisplay viewport being rendered.
	const FString ViewportId;
	const IDisplayClusterViewportManager* ViewportManager;

	FDisplayClusterSceneViewExtensionContext()
		: FSceneViewExtensionContext()
	{ }

	FDisplayClusterSceneViewExtensionContext(FViewport* InViewport, IDisplayClusterViewportManager* InViewportManager, const FString& InViewportId)
		: FSceneViewExtensionContext(InViewport)
		, ViewportId(InViewportId)
		, ViewportManager(InViewportManager)
	{ }

	FDisplayClusterSceneViewExtensionContext(FSceneInterface* InScene, IDisplayClusterViewportManager* InViewportManager, const FString& InViewportId)
		: FSceneViewExtensionContext(InScene)
		, ViewportId(InViewportId)
		, ViewportManager(InViewportManager)
	{ }
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "Render/Viewport/IDisplayClusterViewportConfiguration.h"

/**
 * Contains information about the context in which this scene view extension will be used.
 */
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
	FDisplayClusterSceneViewExtensionContext()
		: FSceneViewExtensionContext()
	{ }

	FDisplayClusterSceneViewExtensionContext(FViewport* InViewport, const TSharedRef<IDisplayClusterViewportConfiguration, ESPMode::ThreadSafe>& InConfiguration, const FString& InViewportId)
		: FSceneViewExtensionContext(InViewport)
		, ViewportId(InViewportId)
		, Configuration(InConfiguration)
	{ }

	FDisplayClusterSceneViewExtensionContext(FSceneInterface* InScene, const TSharedRef<IDisplayClusterViewportConfiguration, ESPMode::ThreadSafe>& InConfiguration, const FString& InViewportId)
		: FSceneViewExtensionContext(InScene)
		, ViewportId(InViewportId)
		, Configuration(InConfiguration)
	{ }

public:
	// The id of the nDisplay viewport being rendered.
	const FString ViewportId;

	// Reference to viewport manager
	const TSharedPtr<IDisplayClusterViewportConfiguration, ESPMode::ThreadSafe> Configuration;
};

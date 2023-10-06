// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"

class IDisplayClusterViewportManager;

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

	FDisplayClusterSceneViewExtensionContext(FViewport* InViewport, const TSharedPtr<IDisplayClusterViewportManager, ESPMode::ThreadSafe>& InViewportManager, const FString& InViewportId)
		: FSceneViewExtensionContext(InViewport)
		, ViewportId(InViewportId)
		, ViewportManagerWeakPtr(InViewportManager)
	{ }

	FDisplayClusterSceneViewExtensionContext(FSceneInterface* InScene, const TSharedPtr<IDisplayClusterViewportManager, ESPMode::ThreadSafe>& InViewportManager, const FString& InViewportId)
		: FSceneViewExtensionContext(InScene)
		, ViewportId(InViewportId)
		, ViewportManagerWeakPtr(InViewportManager)
	{ }

	/** Returns a pointer to the DC ViewportManager used for this VE context. */
	IDisplayClusterViewportManager* GetViewportManager() const
	{
		return ViewportManagerWeakPtr.IsValid() ? ViewportManagerWeakPtr.Pin().Get() : nullptr;
	}

	/** Returns the name of the DC viewport used for this VE context. */
	const FString& GetViewportId() const
	{
		return ViewportId;
	}

private:
	// The id of the nDisplay viewport being rendered.
	const FString ViewportId;

	// Reference to viewport manager
	const TWeakPtr<IDisplayClusterViewportManager, ESPMode::ThreadSafe> ViewportManagerWeakPtr;
};

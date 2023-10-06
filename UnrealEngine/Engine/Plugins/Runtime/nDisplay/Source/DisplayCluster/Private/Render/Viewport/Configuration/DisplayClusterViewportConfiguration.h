// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Misc/DisplayClusterObjectRef.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_Enums.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/Containers/DisplayClusterPreviewSettings.h"

class FDisplayClusterViewportManager;
class ADisplayClusterRootActor;
class UDisplayClusterConfigurationData;
struct FDisplayClusterConfigurationRenderFrame;

class FDisplayClusterViewportConfiguration
{
public:
	FDisplayClusterViewportConfiguration(FDisplayClusterViewportManager& InViewportManager);
	~FDisplayClusterViewportConfiguration();

public:
	// Return true, if root actor ref changed
	bool SetRootActor(ADisplayClusterRootActor* InRootActorPtr);
	ADisplayClusterRootActor* GetRootActor() const;

	const FDisplayClusterRenderFrameSettings& GetRenderFrameSettings() const
	{ 
		check(IsInGameThread());

		return RenderFrameSettings;
	}

	bool UpdateConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const FString& InClusterNodeId);
	bool UpdateCustomConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const TArray<FString>& InViewportNames);

#if WITH_EDITOR
	bool UpdatePreviewConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const FString& ClusterNodeId, const FDisplayClusterPreviewSettings& InPreviewSettings);
#endif

private:
	bool ImplUpdateConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const FString& InClusterNodeId, const FDisplayClusterPreviewSettings* InPreviewSettings, const TArray<FString>* InViewportNames);

	void ImplUpdateRenderFrameConfiguration(const FDisplayClusterConfigurationRenderFrame& InRenderFrameConfiguration);
	void ImplPostUpdateRenderFrameConfiguration();

	/** Hide DCRA components for nDisplay rendering.*/
	void ImplUpdateConfigurationVisibility(ADisplayClusterRootActor& InRootActor, const UDisplayClusterConfigurationData& InConfigurationData) const;

	/** Get alpha channel capture mode (for LightCard, ChromaKey).*/
	EDisplayClusterRenderFrameAlphaChannelCaptureMode GetAlphaChannelCaptureMode() const;

	/** Get a pointer to the DC ViewportManager if it still exists. */
	FDisplayClusterViewportManager* GetViewportManager() const
	{
		return ViewportManagerWeakPtr.IsValid() ? ViewportManagerWeakPtr.Pin().Get() : nullptr;
	}

private:
	/** A reference to the owning viewport manager */
	const TWeakPtr<FDisplayClusterViewportManager> ViewportManagerWeakPtr;

	FDisplayClusterActorRef            RootActorRef;
	FDisplayClusterRenderFrameSettings RenderFrameSettings;
};


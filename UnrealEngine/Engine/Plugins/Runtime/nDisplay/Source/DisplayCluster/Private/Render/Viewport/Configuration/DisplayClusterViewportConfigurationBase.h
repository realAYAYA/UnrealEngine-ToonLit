// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

class FDisplayClusterViewportManager;
class FDisplayClusterViewport;

class UDisplayClusterConfigurationData;
class UDisplayClusterConfigurationViewport;

class ADisplayClusterRootActor;

struct FDisplayClusterViewportConfigurationBase
{
public:
	FDisplayClusterViewportConfigurationBase(FDisplayClusterViewportManager& InViewportManager,
		ADisplayClusterRootActor& InRootActor, const UDisplayClusterConfigurationData& InConfigurationData)
		: RootActor(InRootActor)
		, ViewportManager(InViewportManager)
		, ConfigurationData(InConfigurationData)
	{}

public:
	void Update(const FString& ClusterNodeId);
	void Update(const TArray<FString>& InViewportNames, FDisplayClusterRenderFrameSettings& InOutRenderFrameSettings);
	void UpdateClusterNodePostProcess(const FString& ClusterNodeId, const FDisplayClusterRenderFrameSettings& InRenderFrameSettings);

public:
	static bool UpdateViewportConfiguration(FDisplayClusterViewportManager& ViewportManager, ADisplayClusterRootActor& RootActor, FDisplayClusterViewport* DesiredViewport, const UDisplayClusterConfigurationViewport* ConfigurationViewport);

private:
	ADisplayClusterRootActor& RootActor;
	FDisplayClusterViewportManager& ViewportManager;
	const UDisplayClusterConfigurationData& ConfigurationData;
};

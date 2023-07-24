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

protected:
	/**
	 * By default, all postprocess parameters are defined in the FDisplayClusterConfigurationPostprocess structure.
	 * But some post-processes may use their own logic to enable or disable.
	 * 
	 * For example, TextureSharePP will only be enabled with a special condition:
	 * 1. The EnableTextureShare checkbox is checked in the cluster node settings
	 * 2. The TextureShare plugin is enabled for this project.
	 */
	void AddInternalPostprocess(const FString& InPostprocessName);

private:
	ADisplayClusterRootActor& RootActor;
	FDisplayClusterViewportManager& ViewportManager;
	const UDisplayClusterConfigurationData& ConfigurationData;

	/**
	 * The PP is updated from the configuration every frame.
	 * If the post process name is unknown or initialization fails, an error message appears in the log.
	 * To prevent duplicate log error messages in subsequent frames, this PP name is added to the DisabledPostprocessNames list.
	 *
	 * All PPs with names from this list will be ignored.
	 */
	static TArray<FString> DisabledPostprocessNames;

	/**
	 * This post-processing list is updated at runtime. See AddInternalPostprocess()
	 */
	TArray<FString> InternalPostprocessNames;
};

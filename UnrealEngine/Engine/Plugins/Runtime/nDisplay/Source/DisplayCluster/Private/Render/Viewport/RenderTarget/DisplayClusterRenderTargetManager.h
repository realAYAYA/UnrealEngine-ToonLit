// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Viewport/DisplayClusterViewport.h"

class FViewport;
class FDisplayClusterViewport;
class FDisplayClusterRenderTargetResourcesPool;
class FDisplayClusterViewportConfiguration;
class FDisplayClusterRenderFrame;

class FDisplayClusterRenderTargetManager
{
public:
	FDisplayClusterRenderTargetManager(const TSharedRef<FDisplayClusterViewportConfiguration, ESPMode::ThreadSafe>& InConfiguration);
	~FDisplayClusterRenderTargetManager();

	void Release();

public:
	bool AllocateRenderFrameResources(FViewport* InViewport, const TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>>& InViewports, FDisplayClusterRenderFrame& InOutRenderFrame);

protected:
	bool AllocateFrameTargets(const FIntPoint& InViewportSize, FDisplayClusterRenderFrame& InOutRenderFrame);

private:
	TUniquePtr<FDisplayClusterRenderTargetResourcesPool> ResourcesPool;

	// Configuration of the current cluster node
	const TSharedRef<FDisplayClusterViewportConfiguration, ESPMode::ThreadSafe> Configuration;
};

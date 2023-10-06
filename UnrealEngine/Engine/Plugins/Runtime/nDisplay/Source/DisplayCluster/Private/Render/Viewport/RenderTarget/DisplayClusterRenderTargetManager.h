// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Viewport/DisplayClusterViewport.h"

class FViewport;
class FDisplayClusterViewport;
class FDisplayClusterRenderTargetResourcesPool;
class FDisplayClusterViewportManagerProxy;
class FDisplayClusterRenderFrame;
struct FDisplayClusterRenderFrameSettings;

class FDisplayClusterRenderTargetManager
{
public:
	FDisplayClusterRenderTargetManager(FDisplayClusterViewportManagerProxy* InViewportManagerProxy);
	~FDisplayClusterRenderTargetManager();

	void Release();

public:
	bool AllocateRenderFrameResources(FViewport* InViewport, const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, const TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>>& InViewports, FDisplayClusterRenderFrame& InOutRenderFrame);

protected:
	bool AllocateFrameTargets(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, const FIntPoint& InViewportSize, FDisplayClusterRenderFrame& InOutRenderFrame);

private:
	TUniquePtr<FDisplayClusterRenderTargetResourcesPool> ResourcesPool;
};

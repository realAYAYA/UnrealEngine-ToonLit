// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Viewport/DisplayClusterViewport.h"

class FViewport;
class FDisplayClusterViewport;
class FDisplayClusterRenderFrame;
class FDisplayClusterRenderTargetResourcesPool;
class FDisplayClusterViewportManagerProxy;

struct FDisplayClusterRenderFrameSettings;

class FDisplayClusterRenderTargetManager
{
public:
	FDisplayClusterRenderTargetManager(FDisplayClusterViewportManagerProxy* InViewportManagerProxy);
	~FDisplayClusterRenderTargetManager();

	void Release();

public:
	bool AllocateRenderFrameResources(FViewport* InViewport, const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, const TArray<FDisplayClusterViewport*>& InViewports, FDisplayClusterRenderFrame& InOutRenderFrame);

protected:
	bool AllocateFrameTargets(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, const FIntPoint& InViewportSize, FDisplayClusterRenderFrame& InOutRenderFrame);

private:
	TUniquePtr<FDisplayClusterRenderTargetResourcesPool> ResourcesPool;
};

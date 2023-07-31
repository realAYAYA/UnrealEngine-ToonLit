// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FViewport;
class FDisplayClusterViewport;
class FDisplayClusterRenderFrame;
struct FDisplayClusterRenderFrameSettings;
class FViewport;

class FDisplayClusterRenderFrameManager
{
public:
	bool BuildRenderFrame(FViewport* InViewport, const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, const TArray<FDisplayClusterViewport*>& InViewports, FDisplayClusterRenderFrame& OutRenderFrame);

private:
	bool FindFrameTargetRect(FViewport* InViewport, const TArray<FDisplayClusterViewport*>& InViewports, const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, FIntRect& OutFrameTargetRect) const;
	bool BuildSimpleFrame(FViewport* InViewport, const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, const TArray<FDisplayClusterViewport*>& InViewports, FDisplayClusterRenderFrame& OutRenderFrame);
};



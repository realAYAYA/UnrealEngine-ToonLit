// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterRenderTargetResource.h"

class FDisplayClusterViewportManagerProxy;
struct FDisplayClusterRenderFrameSettings;

class FDisplayClusterRenderTargetResourcesPool
{
public:
	FDisplayClusterRenderTargetResourcesPool(FDisplayClusterViewportManagerProxy* InViewportManagerProxy);
	~FDisplayClusterRenderTargetResourcesPool();

	void Release();

public:
	bool BeginReallocateResources(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, class FViewport* InViewport);
	void FinishReallocateResources();

	FDisplayClusterViewportRenderTargetResource* AllocateRenderTargetResource(const FIntPoint& InSize, EPixelFormat CustomPixelFormat);
	FDisplayClusterViewportTextureResource*      AllocateTextureResource(const FIntPoint& InSize, bool bIsRenderTargetable, EPixelFormat CustomPixelFormat, int32 NumMips = 1);

private:
	template <typename TViewportResourceType>
	void ImplBeginReallocateResources(TArray<TViewportResourceType*>& InOutViewportResources);

	template <typename TViewportResourceType>
	void ImplFinishReallocateResources(TArray<TViewportResourceType*>& InOutViewportResources);

	template <typename TViewportResourceType>
	TViewportResourceType* ImplAllocateResource(TArray<TViewportResourceType*>& InOutViewportResources, const FDisplayClusterViewportResourceSettings& InSettings);

	template <typename TViewportResourceType>
	void ImplReleaseResources(TArray<TViewportResourceType*>& InOutViewportResources);

private:
	// Current render resource settings
	FDisplayClusterViewportResourceSettings* ResourceSettings = nullptr;

	// Viewport render resources
	TArray<FDisplayClusterViewportRenderTargetResource*> RenderTargetResources;
	TArray<FDisplayClusterViewportTextureResource*>      TextureResources;

	FDisplayClusterViewportManagerProxy* ViewportManagerProxy = nullptr;
};

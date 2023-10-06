// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DisplayClusterViewportLightCardResource.h"

#include "RHIResources.h"
#include "Templates/SharedPointer.h"

struct FDisplayClusterShaderParameters_UVLightCards;
class FSceneInterface;

/*
 * Manages the rendering of UV light cards for the viewport manager (Render Thread proxy object)
 */
class FDisplayClusterViewportLightCardManagerProxy
	: public TSharedFromThis<FDisplayClusterViewportLightCardManagerProxy, ESPMode::ThreadSafe>
{
public:
	virtual ~FDisplayClusterViewportLightCardManagerProxy();

	/** Update UVLightCard resource. */
	void UpdateUVLightCardResource(const TSharedPtr<FDisplayClusterViewportLightCardResource, ESPMode::ThreadSafe>& InUVLightCardMapResource);

	/** Release UVLightCard resource. */
	void ReleaseUVLightCardResource();

	/** Render UVLightCard. */
	void RenderUVLightCard(FSceneInterface* InSceneInterface, const FDisplayClusterShaderParameters_UVLightCards& InParameters) const;

	/** Get current UVLightCard RHI resource on rendering thread. */
	FRHITexture* GetUVLightCardRHIResource_RenderThread() const;

protected:
	void ImplUpdateUVLightCardResource_RenderThread(const TSharedPtr<FDisplayClusterViewportLightCardResource, ESPMode::ThreadSafe>& InUVLightCardMapResource);
	void ImplReleaseUVLightCardResource_RenderThread();

	void ImplRenderUVLightCard_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneInterface* InScene, const FDisplayClusterShaderParameters_UVLightCards& InParameters) const;

private:
	/** The render thread copy of the pointer to the UV light card map */
	TSharedPtr<FDisplayClusterViewportLightCardResource, ESPMode::ThreadSafe> UVLightCardMapResource;
};

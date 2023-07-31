// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#include "Containers/TextureShareCoreEnums.h"
#include "Blueprints/TextureShareBlueprintContainers.h"

#include "RHI.h"
#include "RHICommandList.h"
#include "RHIResources.h"

struct FProxyResourcesData;
class UTextureShareObject;
class ITextureShareObjectProxy;

/**
 * TextureShare object logic for the render thread
 */
struct FTextureShareWorldSubsystemObjectProxy
{
public:
	FTextureShareWorldSubsystemObjectProxy(const TSharedPtr<ITextureShareObjectProxy, ESPMode::ThreadSafe>& InObjectProxy);

	// Update texturesahre object logic from UObject data
	bool Update(const UTextureShareObject* InTextureShareObject);

private:
	void Update_RenderThread(FRHICommandListImmediate& RHICmdList, TSharedPtr<FProxyResourcesData> ProxyData);
	bool UpdateResources_RenderThread(FRHICommandListImmediate& RHICmdList, TSharedPtr<FProxyResourcesData> ProxyData);
	bool UpdateFrameProxyBackbuffer_RenderThread(FRHICommandListImmediate& RHICmdList, const FTexture2DRHIRef& InBackbuffer);

private:
	TSharedPtr<ITextureShareObjectProxy, ESPMode::ThreadSafe> ObjectProxy;
};


// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LightmapStorage.h"

namespace GPULightmass
{

class FLightmapRenderer;

class FLightmapPreviewVirtualTexture : public IVirtualTexture
{
public:
	FLightmapPreviewVirtualTexture(FLightmapRenderStateRef LightmapRenderState, FLightmapRenderer* Renderer);

	virtual FVTRequestPageResult RequestPageData(
		const FVirtualTextureProducerHandle& ProducerHandle,
		uint8 LayerMask,
		uint8 vLevel,
		uint64 vAddress,
		EVTRequestPagePriority Priority
	) override;

	virtual IVirtualTextureFinalizer* ProducePageData(
		FRHICommandListImmediate& RHICmdList,
		ERHIFeatureLevel::Type FeatureLevel,
		EVTProducePageFlags Flags,
		const FVirtualTextureProducerHandle& InProducerHandle,
		uint8 LayerMask,
		uint8 vLevel,
		uint64 vAddress,
		uint64 RequestHandle,
		const FVTProduceTargetLayer* TargetLayers
	) override;

	IAllocatedVirtualTexture* AllocatedVT;
	FVirtualTextureProducerHandle ProducerHandle;

private:
	FLightmapRenderStateRef LightmapRenderState;
	FLightmapRenderer* LightmapRenderer;
};

}

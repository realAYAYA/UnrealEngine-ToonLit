#pragma once
#include "RenderGraphUtils.h"

struct FToonTextures
{
	// Initializes the scene textures structure in the FViewFamilyInfo
	void InitializeViewFamily(FRDGBuilder& GraphBuilder, FViewFamilyInfo& ViewFamily);
	
	static EPixelFormat GetTBufferFFormatAndCreateFlags(ETextureCreateFlags& OutCreateFlags);

	// Configures an array of render targets for the TBuffer pass.
	uint32 GetTBufferRenderTargets(
		TArrayView<FTextureRenderTargetBinding> RenderTargets) const;
	uint32 GetTBufferRenderTargets(
		ERenderTargetLoadAction LoadAction,
		FRenderTargetBindingSlots& RenderTargets) const;
	
	FRDGTextureRef TBufferA{};
};

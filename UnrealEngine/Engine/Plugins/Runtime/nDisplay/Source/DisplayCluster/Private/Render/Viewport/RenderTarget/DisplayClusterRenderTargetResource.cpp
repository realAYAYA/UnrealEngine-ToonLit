// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"

#include "RHI.h"
#include "RenderResource.h"
#include "UnrealClient.h"

////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportTextureResource
////////////////////////////////////////////////////////////////////////
FDisplayClusterViewportTextureResource::FDisplayClusterViewportTextureResource(const FDisplayClusterViewportResourceSettings& InResourceSettings)
	: FDisplayClusterViewportResource(InResourceSettings)
{
	bSRGB = EnumHasAnyFlags(GetResourceSettings().GetResourceFlags(), EDisplayClusterViewportResourceSettingsFlags::ShouldUseSRGB);
	bGreyScaleFormat = false;
}

void FDisplayClusterViewportTextureResource::InitRHI(FRHICommandListBase&)
{
	FTexture2DRHIRef NewTextureRHI;

	if (GetResourceSettings().GetNumMips() > 1)
	{
		FTexture2DRHIRef DummyTextureRHI;
		ImplInitDynamicRHI_RenderTargetResource2D(NewTextureRHI, DummyTextureRHI);
	}
	else
	{
		ImplInitDynamicRHI_TextureResource2D(NewTextureRHI);
	}

	TextureRHI = (FTextureRHIRef&)NewTextureRHI;
}

////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportRenderTargetResource
////////////////////////////////////////////////////////////////////////
void FDisplayClusterViewportRenderTargetResource::InitRHI(FRHICommandListBase&)
{
	// Create RTT and shader resources
	FTexture2DRHIRef NewTextureRHI;
	ImplInitDynamicRHI_RenderTargetResource2D(RenderTargetTextureRHI, NewTextureRHI);
	TextureRHI = (FTextureRHIRef&)NewTextureRHI;

	// Create the sampler state RHI resource.
	FSamplerStateInitializerRHI SamplerStateInitializer
	(
		SF_Bilinear,
		AM_Clamp,
		AM_Clamp,
		AM_Clamp
	);
	SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"

#include "RHI.h"
#include "RenderResource.h"
#include "UnrealClient.h"

//------------------------------------------------------------------------------------------------
// FDisplayClusterViewportResource
//------------------------------------------------------------------------------------------------
FDisplayClusterViewportResource::FDisplayClusterViewportResource(const FDisplayClusterViewportResourceSettings& InResourceSettings)
	: ViewportResourceSettings(InResourceSettings)
{
	bSRGB = ViewportResourceSettings.bShouldUseSRGB;
	bGreyScaleFormat = false;
	bIgnoreGammaConversions = true;
}

void FDisplayClusterViewportResource::ImplInitDynamicRHI_RenderTargetResource2D(FTexture2DRHIRef& OutRenderTargetTextureRHI, FTexture2DRHIRef& OutTextureRHI)
{
	ETextureCreateFlags CreateFlags = TexCreate_Dynamic;

	// -- we will be manually copying this cross GPU, tell render graph not to
	CreateFlags |= TexCreate_MultiGPUGraphIgnore;

	// reflect srgb from settings
	if (bSRGB)
	{
		CreateFlags |= TexCreate_SRGB;
	}

	uint32 NumMips = 1;
	if (ViewportResourceSettings.NumMips > 1)
	{
		// Create nummips texture!
		CreateFlags |= TexCreate_GenerateMipCapable | TexCreate_UAV;
		NumMips = ViewportResourceSettings.NumMips;
	}

	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("DisplayClusterViewportRenderTargetResource"))
		.SetExtent(GetSizeX(), GetSizeY())
		.SetFormat(ViewportResourceSettings.Format)
		.SetNumMips(NumMips)
		.SetFlags(CreateFlags | ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
		.SetInitialState(ERHIAccess::SRVMask)
		.SetClearValue(FClearValueBinding::Black);

	OutRenderTargetTextureRHI = OutTextureRHI = RHICreateTexture(Desc);
}

void FDisplayClusterViewportResource::ImplInitDynamicRHI_TextureResource2D(FTexture2DRHIRef& OutTextureRHI)
{
	FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("DisplayClusterViewportTextureResource"), GetSizeX(), GetSizeY(), ViewportResourceSettings.Format)
		.SetClearValue(FClearValueBinding::Black)
		.SetFlags(ETextureCreateFlags::Dynamic | ETextureCreateFlags::ShaderResource)
		// -- we will be manually copying this cross GPU, tell render graph not to
		.AddFlags(ETextureCreateFlags::MultiGPUGraphIgnore)
		.SetInitialState(ERHIAccess::SRVMask);

	// reflect srgb from settings
	if (bSRGB)
	{
		Desc.AddFlags(ETextureCreateFlags::SRGB);
	}
	if (ViewportResourceSettings.bIsRenderTargetable)
	{
		Desc.AddFlags(ETextureCreateFlags::RenderTargetable);
	}
	else
	{
		Desc.AddFlags(ETextureCreateFlags::ResolveTargetable);
	}

	OutTextureRHI = RHICreateTexture(Desc);
}

//------------------------------------------------------------------------------------------------
// FDisplayClusterViewportTextureResource
//------------------------------------------------------------------------------------------------
void FDisplayClusterViewportTextureResource::InitDynamicRHI()
{
	FTexture2DRHIRef NewTextureRHI;

	if (GetResourceSettingsConstRef().NumMips > 1)
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

//------------------------------------------------------------------------------------------------
// FDisplayClusterViewportRenderTargetResource
//------------------------------------------------------------------------------------------------
void FDisplayClusterViewportRenderTargetResource::InitDynamicRHI()
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

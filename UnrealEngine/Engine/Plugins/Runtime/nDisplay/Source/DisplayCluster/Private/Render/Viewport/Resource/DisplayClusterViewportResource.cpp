// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Resource/DisplayClusterViewportResource.h"

#include "RHI.h"
#include "RenderResource.h"
#include "UnrealClient.h"

////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportResource
////////////////////////////////////////////////////////////////////////
FDisplayClusterViewportResource::~FDisplayClusterViewportResource()
{
	// resource must be released in the rendering thread before the destructor is called
	check(!EnumHasAnyFlags(ResourceState, EDisplayClusterViewportResourceState::Initialized));
}

void FDisplayClusterViewportResource::ImplInitDynamicRHI_RenderTargetResource2D(FTexture2DRHIRef& OutRenderTargetTextureRHI, FTexture2DRHIRef& OutTextureRHI)
{
	ETextureCreateFlags CreateFlags = TexCreate_Dynamic;

	// -- we will be manually copying this cross GPU, tell render graph not to
	CreateFlags |= TexCreate_MultiGPUGraphIgnore;

	// reflect srgb from settings
	if (EnumHasAnyFlags(ResourceSettings.GetResourceFlags(), EDisplayClusterViewportResourceSettingsFlags::ShouldUseSRGB))
	{
		CreateFlags |= TexCreate_SRGB;
	}

	uint32 NumMips = 1;
	if (ResourceSettings.GetNumMips() > 1)
	{
		// Create nummips texture!
		CreateFlags |= TexCreate_GenerateMipCapable | TexCreate_UAV;
		NumMips = ResourceSettings.GetNumMips();
	}

	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("DisplayClusterViewportRenderTargetResource"))
		.SetExtent(ResourceSettings.GetSizeXY().X, ResourceSettings.GetSizeXY().Y)
		.SetFormat(ResourceSettings.GetFormat())
		.SetNumMips(NumMips)
		.SetFlags(CreateFlags | ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
		.SetInitialState(ERHIAccess::SRVMask)
		.SetClearValue(FClearValueBinding::Black);

	OutRenderTargetTextureRHI = OutTextureRHI = RHICreateTexture(Desc);
}

void FDisplayClusterViewportResource::ImplInitDynamicRHI_TextureResource2D(FTexture2DRHIRef& OutTextureRHI)
{
	FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("DisplayClusterViewportTextureResource"), ResourceSettings.GetSizeXY().X, ResourceSettings.GetSizeXY().Y, ResourceSettings.GetFormat())
		.SetClearValue(FClearValueBinding::Black)
		.SetFlags(ETextureCreateFlags::Dynamic | ETextureCreateFlags::ShaderResource)
		// -- we will be manually copying this cross GPU, tell render graph not to
		.AddFlags(ETextureCreateFlags::MultiGPUGraphIgnore)
		.SetInitialState(ERHIAccess::SRVMask);

	// reflect srgb from settings
	if (EnumHasAnyFlags(ResourceSettings.GetResourceFlags(), EDisplayClusterViewportResourceSettingsFlags::ShouldUseSRGB))
	{
		Desc.AddFlags(ETextureCreateFlags::SRGB);
	}

	// reflect RenderTargetable flags
	if (EnumHasAnyFlags(ResourceSettings.GetResourceFlags(), EDisplayClusterViewportResourceSettingsFlags::RenderTargetableTexture))
	{
		Desc.AddFlags(ETextureCreateFlags::RenderTargetable);
	}
	else
	{
		Desc.AddFlags(ETextureCreateFlags::ResolveTargetable);
	}

	OutTextureRHI = RHICreateTexture(Desc);
}

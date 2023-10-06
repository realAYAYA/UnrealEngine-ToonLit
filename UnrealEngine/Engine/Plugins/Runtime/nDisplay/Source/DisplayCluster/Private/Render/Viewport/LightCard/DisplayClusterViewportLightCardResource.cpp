// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportLightCardResource.h"

///////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportLightCardResource
///////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterViewportLightCardResource::InitRHI(FRHICommandListBase&)
{
	ETextureCreateFlags CreateFlags = TexCreate_Dynamic;
	CreateFlags |= TexCreate_MultiGPUGraphIgnore;

	const FRHITextureCreateDesc Desc = FRHITextureCreateDesc::Create2D(TEXT("DisplayClusterViewportLightCardResource"))
		.SetExtent(GetSizeX(), GetSizeY())
		.SetFormat(PF_FloatRGBA)
		.SetFlags(CreateFlags | ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
		.SetInitialState(ERHIAccess::SRVMask)
		.SetClearValue(FClearValueBinding::Transparent);

	RenderTargetTextureRHI = TextureRHI = RHICreateTexture(Desc);
}

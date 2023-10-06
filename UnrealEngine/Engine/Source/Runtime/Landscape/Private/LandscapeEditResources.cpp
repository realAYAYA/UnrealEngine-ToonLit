// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditResources.h"

#include "Engine/Texture2D.h"
#include "LandscapePrivate.h"
#include "RenderingThread.h"
#include "RenderGraph.h"
#include "Rendering/Texture2DResource.h"


// ----------------------------------------------------------------------------------

FLandscapeTexture2DResource::FLandscapeTexture2DResource(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, uint32 bInNumMips, bool bInNeedUAVs, bool bInNeedSRV)
	: SizeX(InSizeX)
	, SizeY(InSizeY)
	, Format(InFormat)
	, NumMips(IntCastChecked<uint8>(bInNumMips))
	, bCreateUAVs(bInNeedUAVs)
	, bCreateSRV(bInNeedSRV)
{

}

void FLandscapeTexture2DResource::InitRHI(FRHICommandListBase& RHICmdList)
{
	FTextureResource::InitRHI(RHICmdList);

	FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("FLandscapeTexture2DResource"), SizeX, SizeY, Format)
		.SetNumMips(static_cast<uint8>(NumMips));

	if (bCreateUAVs)
	{
		Desc.AddFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::UAV);
	}

	if (bCreateSRV)
	{
		Desc.AddFlags(ETextureCreateFlags::ShaderResource);
	}

	TextureRHI = RHICreateTexture(Desc);

	if (bCreateUAVs)
	{
		TextureUAVs.Reserve(NumMips);
		for (uint32 MipLevel = 0; MipLevel < NumMips; ++MipLevel)
		{
			TextureUAVs.Add(RHICmdList.CreateUnorderedAccessView(TextureRHI, FRHIViewDesc::CreateTextureUAV()
				.SetDimensionFromTexture(TextureRHI)
				.SetMipLevel(uint8(MipLevel))
			));
		}
	}

	if (bCreateSRV)
	{
		TextureSRV = RHICmdList.CreateShaderResourceView(TextureRHI, /*MipLevel = */0, static_cast<uint8>(NumMips), Format);
	}
}

void FLandscapeTexture2DResource::ReleaseRHI()
{
	FTextureResource::ReleaseRHI();

	if (bCreateUAVs)
	{
		for (FUnorderedAccessViewRHIRef& TextureUAV : TextureUAVs)
		{
			TextureUAV.SafeRelease();
		}
		TextureUAVs.Empty();
	}
}

FUnorderedAccessViewRHIRef FLandscapeTexture2DResource::GetTextureUAV(uint32 InMipLevel) const
{
	check(bCreateUAVs && (InMipLevel < NumMips));
	check(TextureUAVs.Num() == NumMips);
	return TextureUAVs[InMipLevel];
}

FShaderResourceViewRHIRef FLandscapeTexture2DResource::GetTextureSRV() const
{
	check(bCreateSRV);
	return TextureSRV;
}

// ----------------------------------------------------------------------------------

FLandscapeTexture2DArrayResource::FLandscapeTexture2DArrayResource(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, EPixelFormat InFormat, uint32 InNumMips, bool bInNeedUAVs, bool bInNeedSRV)
	: SizeX(InSizeX)
	, SizeY(InSizeY)
	, SizeZ(InSizeZ)
	, Format(InFormat)
	, NumMips(InNumMips)
	, bCreateUAVs(bInNeedUAVs)
    , bCreateSRV(bInNeedSRV)
{

}

void FLandscapeTexture2DArrayResource::InitRHI(FRHICommandListBase& RHICmdList)
{
	FTextureResource::InitRHI(RHICmdList);
    
	FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2DArray(TEXT("FLandscapeTexture2DArrayResource"), SizeX, SizeY, SizeZ, Format)
        .SetNumMips(static_cast<uint8>(NumMips));

    Desc.AddFlags(ETextureCreateFlags::NoTiling | ETextureCreateFlags::OfflineProcessed);
    
    if (bCreateUAVs)
    {
        Desc.AddFlags(ETextureCreateFlags::UAV);
    }

    if (bCreateSRV)
    {
        Desc.AddFlags(ETextureCreateFlags::ShaderResource);
    }
    
    TextureRHI = RHICreateTexture(Desc);

	if (bCreateUAVs)
	{
		TextureUAVs.Reserve(NumMips);
		for (uint32 MipLevel = 0; MipLevel < NumMips; ++MipLevel)
		{
			TextureUAVs.Add(RHICmdList.CreateUnorderedAccessView(TextureRHI, FRHIViewDesc::CreateTextureUAV()
				.SetDimensionFromTexture(TextureRHI)
				.SetMipLevel(MipLevel)
			));
		}
	}

	if (bCreateSRV)
	{
		TextureSRV = RHICmdList.CreateShaderResourceView(TextureRHI, /*MipLevel = */0, static_cast<uint8>(NumMips), Format);
	}
}

void FLandscapeTexture2DArrayResource::ReleaseRHI()
{
	FTextureResource::ReleaseRHI();

	if (bCreateUAVs)
	{
		for (FUnorderedAccessViewRHIRef& TextureUAV : TextureUAVs)
		{
			TextureUAV.SafeRelease();
		}
		TextureUAVs.Empty();
	}
}

FUnorderedAccessViewRHIRef FLandscapeTexture2DArrayResource::GetTextureUAV(uint32 InMipLevel) const
{
	check(bCreateUAVs && (InMipLevel < NumMips));
	check(TextureUAVs.Num() == NumMips);
	return TextureUAVs[InMipLevel];
}

FShaderResourceViewRHIRef FLandscapeTexture2DArrayResource::GetTextureSRV() const
{
	check(bCreateSRV);
	return TextureSRV;
}


// ----------------------------------------------------------------------------------

void TrackLandscapeRDGTextures(FRDGBuilder& GraphBuilder, TMap<FTexture2DResource*, FLandscapeRDGTrackedTexture>& TrackedTextures)
{
	// Register all these to the graph and mark those that will be written to it as targetable + create UAV/SRV : 
	for (auto& ItTrackedTexture : TrackedTextures)
	{
		FLandscapeRDGTrackedTexture& TrackedTexture = ItTrackedTexture.Value;
		check(TrackedTexture.TextureResource != nullptr);

		// We need a debug name that will last until the graph is executed: 
		TrackedTexture.DebugName = GraphBuilder.AllocObject<FString>(TrackedTexture.TextureResource->GetTextureName().ToString());

		// Register the texture to the GraphBuilder : we'll read from / write to it like a render target :
		TrackedTexture.ExternalTextureRef = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(TrackedTexture.TextureResource->TextureRHI, **TrackedTexture.DebugName));

		if (TrackedTexture.bNeedsScratch)
		{
			FString* ScratchTextureName = GraphBuilder.AllocObject<FString>(FString::Format(TEXT("{0}_Scratch"), { *TrackedTexture.DebugName }));
			// We'll work on a targetable copy of the original texture so that we can write to it from a shader (we'll copy to the original one at the end of the process with a CopyTexture): 
			FRDGTextureDesc ScratchTextureDesc = TrackedTexture.ExternalTextureRef->Desc;
			ScratchTextureDesc.Flags |= ETextureCreateFlags::RenderTargetable;
			TrackedTexture.ScratchTextureRef = GraphBuilder.CreateTexture(ScratchTextureDesc, **ScratchTextureName);

			for (int32 MipLevel = 0; MipLevel < ScratchTextureDesc.NumMips; ++MipLevel)
			{
				TrackedTexture.ScratchTextureMipsSRVRefs.Add(GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(TrackedTexture.ScratchTextureRef, MipLevel)));
			}

			// We only need to copy the first mip : 
			FRHICopyTextureInfo CopyTextureInfo;
			CopyTextureInfo.NumMips = 1;
			CopyTextureInfo.Size = FIntVector(TrackedTexture.ExternalTextureRef->Desc.GetSize().X, TrackedTexture.ExternalTextureRef->Desc.GetSize().Y, 0);
			AddCopyTexturePass(GraphBuilder, TrackedTexture.ExternalTextureRef, TrackedTexture.ScratchTextureRef, CopyTextureInfo);
		}

		if (TrackedTexture.bNeedsSRV)
		{
			TrackedTexture.ExternalTextureSRVRef = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(TrackedTexture.ExternalTextureRef));
		}
	}
}

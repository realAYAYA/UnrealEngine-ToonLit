// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureResource.h"


// Custom Resources for edit layers landscapes :

// ----------------------------------------------------------------------------------
class FLandscapeTexture2DResource : public FTextureResource
{
public:
	FLandscapeTexture2DResource(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, uint32 InNumMips, bool bInNeedUAVs, bool bInNeedSRV);

	virtual uint32 GetSizeX() const override { return SizeX; }
	virtual uint32 GetSizeY() const override { return SizeY; }

	/** Called when the resource is initialized. This is only called by the rendering thread. */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	FUnorderedAccessViewRHIRef GetTextureUAV(uint32 InMipLevel) const;
	FShaderResourceViewRHIRef GetTextureSRV() const;

private:
	uint32 SizeX;
	uint32 SizeY;
	EPixelFormat Format;
	uint32 NumMips;
	bool bCreateUAVs;
	bool bCreateSRV;
	TArray<FUnorderedAccessViewRHIRef> TextureUAVs;
	FShaderResourceViewRHIRef TextureSRV;
};

// ----------------------------------------------------------------------------------
class FLandscapeTexture2DArrayResource : public FTextureResource
{
public:
	FLandscapeTexture2DArrayResource(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, EPixelFormat InFormat, uint32 InNumMips, bool bInNeedUAVs, bool bInNeedSRV);

	virtual uint32 GetSizeX() const override { return SizeX; }
	virtual uint32 GetSizeY() const override { return SizeY;}
	virtual uint32 GetSizeZ() const { return SizeZ; }

	/** Called when the resource is initialized. This is only called by the rendering thread. */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	FUnorderedAccessViewRHIRef GetTextureUAV(uint32 InMipLevel) const;
	FShaderResourceViewRHIRef GetTextureSRV() const;

private:
	uint32 SizeX;
	uint32 SizeY;
	uint32 SizeZ;
	EPixelFormat Format;
	uint32 NumMips;
	bool bCreateUAVs;
	bool bCreateSRV;
	TArray<FUnorderedAccessViewRHIRef> TextureUAVs;
	FShaderResourceViewRHIRef TextureSRV;
};

// ----------------------------------------------------------------------------------
// Describes a texture and an associated subregion (for heightmap texture sharing), and, optionally, a single channel (for weightmap texture sharing) :
struct FTexture2DResourceSubregion
{
	FTexture2DResourceSubregion() = default;

	FTexture2DResourceSubregion(FTexture2DResource* InTexture, const FIntRect& InSubregion, int32 InChannelIndex = INDEX_NONE)
		: Texture(InTexture)
		, Subregion(InSubregion)
		, ChannelIndex(InChannelIndex)
	{}

	FTexture2DResource* Texture = nullptr;
	FIntRect Subregion;
	int32 ChannelIndex = INDEX_NONE;
};


// ----------------------------------------------------------------------------------
// Struct that helps tracking external textures in the render graph
struct FLandscapeRDGTrackedTexture
{
	FLandscapeRDGTrackedTexture(FTexture2DResource* InTextureResource)
		: TextureResource(InTextureResource)
	{}

	/** External texture resource that needs tracking */
	FTexture2DResource* TextureResource = nullptr;
	/** If bNeedsScratch is set to true, a sister copy of the the texture will be allocated: FLandscapeRDGTrackedTexture is meant to track non-render targetable and UAV-less textures (UTextures...), so any change to 
	 the texture needs to be done via a texture copy (scratch -> original) */
	bool bNeedsScratch = false;
	/** If bNeedsSRV is set to true, the registration of the texture in the render graph will also produce a FRDGTextureSRVRef (ExternalTextureSRVRef) in order to access this texture as a SRV */
	bool bNeedsSRV = false;
	/** Debug name for this texture (transient memory : will last for the lifetime of the GraphBuilder) */
	FString* DebugName = nullptr;
	/** If bNeedsScratch is true, contains the RDG ref of the allocated intermediate scratch texture */
	FRDGTextureRef ScratchTextureRef = nullptr;
	/** If bNeedsScratch is true, contains the RDG refs of the intermediate scratch texture SRVs (one SRV per mip level) */
	TArray<FRDGTextureSRVRef> ScratchTextureMipsSRVRefs; 
	/** RDG ref for the TextureResource */
	FRDGTextureRef ExternalTextureRef = nullptr;
	/** If bNeedsSRV is true, RDG ref for the TextureResource's SRV */
	FRDGTextureSRVRef ExternalTextureSRVRef = nullptr;
};

/**
 * Register all textures we will write into or read from in the render graph :
 */
void TrackLandscapeRDGTextures(FRDGBuilder& GraphBuilder, TMap<FTexture2DResource*, FLandscapeRDGTrackedTexture>& TrackedTextures);
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "VirtualTexturing.h"
#include "VT/RuntimeVirtualTextureEnum.h"

enum class ERuntimeVirtualTextureMaterialType : uint8;
class FRHITexture;
class FSceneInterface;

/** IVirtualTextureFinalizer implementation that renders the virtual texture pages on demand. */
class FRuntimeVirtualTextureFinalizer : public IVirtualTextureFinalizer
{
public:
	FRuntimeVirtualTextureFinalizer(
		FVTProducerDescription const& InDesc, 
		uint32 InProducerId, 
		ERuntimeVirtualTextureMaterialType InMaterialType, 
		bool InClearTextures, 
		FSceneInterface* InScene, 
		FTransform const& InUVToWorld,
		FBox const& InWorldBounds);

	virtual ~FRuntimeVirtualTextureFinalizer() {}

	/** A description for a single tile to render. */
	struct FTileEntry
	{
		FVTProduceTargetLayer Targets[RuntimeVirtualTexture::MaxTextureLayers];
		uint64 vAddress = 0;
		uint8 vLevel = 0;
	};

	/** Returns false if we don't yet have everything we need to render a VT page. */
	bool IsReady();

	/** Does some one time work at the first call to set up the Producer */
	void InitProducer(const FVirtualTextureProducerHandle& ProducerHandle);

	/** Add a tile to the finalize queue. */
	void AddTile(FTileEntry& Tile);

	//~ Begin IVirtualTextureFinalizer Interface.
	virtual void Finalize(FRDGBuilder& GraphBuilder) override;
	//~ End IVirtualTextureFinalizer Interface.

private:
	/** Description of our virtual texture. */
	const FVTProducerDescription Desc;
	/** Producer index used once to build the RuntimeVirtualTextureMask for any rendering. */
	uint32 ProducerId;
	/** The mask of runtime virtual textures in the scene that we should render to. */
	uint32 RuntimeVirtualTextureMask;
	/** Contents of virtual texture layer stack. */
	ERuntimeVirtualTextureMaterialType MaterialType;
	/** Clear before render flag. */
	bool bClearTextures;
	/** Scene that the virtual texture is placed within. */
	FSceneInterface* Scene;
	/** Transform from UV space to world space. */
	FTransform UVToWorld;
	/** Bounds of runtime virtual texture volume in world space. */
	FBox WorldBounds;
	/** Array of tiles in the queue to finalize. */
	TArray<FTileEntry> Tiles;
};

/** IVirtualTexture implementation that is handling runtime rendered page data requests. */
class FRuntimeVirtualTextureProducer : public IVirtualTexture
{
public:
	RENDERER_API FRuntimeVirtualTextureProducer(
		FVTProducerDescription const& InDesc, 
		uint32 InProducerId, 
		ERuntimeVirtualTextureMaterialType InMaterialType, 
		bool InClearTextures, 
		FSceneInterface* InScene, 
		FTransform const& InUVToWorld,
		FBox const& InWorldBounds);
	
	virtual ~FRuntimeVirtualTextureProducer() {}

	//~ Begin IVirtualTexture Interface.
	virtual bool IsPageStreamed(uint8 vLevel, uint32 vAddress) const override
	{
		return false;
	}

	virtual FVTRequestPageResult RequestPageData(
		FRHICommandList& RHICmdList,
		const FVirtualTextureProducerHandle& ProducerHandle,
		uint8 LayerMask,
		uint8 vLevel,
		uint64 vAddress,
		EVTRequestPagePriority Priority
	) override;

	virtual IVirtualTextureFinalizer* ProducePageData(
		FRHICommandList& RHICmdList,
		ERHIFeatureLevel::Type FeatureLevel,
		EVTProducePageFlags Flags,
		const FVirtualTextureProducerHandle& ProducerHandle,
		uint8 LayerMask,
		uint8 vLevel,
		uint64 vAddress,
		uint64 RequestHandle,
		const FVTProduceTargetLayer* TargetLayers
	) override;
	//~ End IVirtualTexture Interface.

private:
	FRuntimeVirtualTextureFinalizer Finalizer;
};

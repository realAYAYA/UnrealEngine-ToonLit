// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "VirtualTexturing.h"

enum class ERuntimeVirtualTextureMaterialType;
class FRHITexture;
class FSceneInterface;

/** IVirtualTextureFinalizer implementation that renders the virtual texture pages on demand. */
class FRuntimeVirtualTextureFinalizer : public IVirtualTextureFinalizer
{
public:
	FRuntimeVirtualTextureFinalizer(FVTProducerDescription const& InDesc, ERuntimeVirtualTextureMaterialType InMaterialType, FSceneInterface* InScene, FTransform const& InUVToWorld);
	virtual ~FRuntimeVirtualTextureFinalizer() {}

	/** A description for a single tile to render. */
	struct FTileEntry
	{
		FRHITexture2D* Texture0 = nullptr;
		FRHITexture2D* Texture1 = nullptr;
		int32 DestX0 = 0;
		int32 DestY0 = 0;
		int32 DestX1 = 0;
		int32 DestY1 = 0;
		uint32 vAddress = 0;
		uint8 vLevel = 0;
	};

	/** Returns false if we don't yet have everything we need to render a VT page. */
	bool IsReady();

	/** Add a tile to the finalize queue. */
	void AddTile(FTileEntry& Tile);

	//~ Begin IVirtualTextureFinalizer Interface.
	virtual void Finalize(FRDGBuilder& GraphBuilder) override;
	//~ End IVirtualTextureFinalizer Interface.

private:
	/** Description of our virtual texture. */
	const FVTProducerDescription Desc;
	/** Contents of virtual texture layer stack. */
	ERuntimeVirtualTextureMaterialType MaterialType;
	/** Scene that the virtual texture is placed within. */
	FSceneInterface* Scene;
	/** Transform from UV space to world space. */
	FTransform UVToWorld;
	/** Array of tiles in the queue to finalize. */
	TArray<FTileEntry> Tiles;
};

/** IVirtualTexture implementation that is handling runtime rendered page data requests. */
class FRuntimeVirtualTextureProducer : public IVirtualTexture
{
public:
	RENDERER_API FRuntimeVirtualTextureProducer(FVTProducerDescription const& InDesc, ERuntimeVirtualTextureMaterialType InMaterialType, FSceneInterface* InScene, FTransform const& InUVToWorld);
	RENDERER_API virtual ~FRuntimeVirtualTextureProducer() {}

	//~ Begin IVirtualTexture Interface.
	virtual FVTRequestPageResult RequestPageData(
		const FVirtualTextureProducerHandle& ProducerHandle,
		uint8 LayerMask,
		uint8 vLevel,
		uint32 vAddress,
		EVTRequestPagePriority Priority
	) override;

	virtual IVirtualTextureFinalizer* ProducePageData(
		FRHICommandListImmediate& RHICmdList,
		ERHIFeatureLevel::Type FeatureLevel,
		EVTProducePageFlags Flags,
		const FVirtualTextureProducerHandle& ProducerHandle,
		uint8 LayerMask,
		uint8 vLevel,
		uint32 vAddress,
		uint64 RequestHandle,
		const FVTProduceTargetLayer* TargetLayers
	) override;
	//~ End IVirtualTexture Interface.

private:
	FRuntimeVirtualTextureFinalizer Finalizer;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeVirtualTextureProducer.h"

#include "RendererInterface.h"
#include "ScenePrivate.h"
#include "VT/RuntimeVirtualTextureRender.h"
#include "VT/RuntimeVirtualTextureSceneProxy.h"


FRuntimeVirtualTextureFinalizer::FRuntimeVirtualTextureFinalizer(
	FVTProducerDescription const& InDesc, 
	uint32 InProducerId, 
	ERuntimeVirtualTextureMaterialType InMaterialType, 
	bool InClearTextures, 
	FSceneInterface* InScene, 
	FTransform const& InUVToWorld,
	FBox const& InWorldBounds)
	: Desc(InDesc)
	, ProducerId(InProducerId)
	, RuntimeVirtualTextureMask(0)
	, MaterialType(InMaterialType)
	, bClearTextures(InClearTextures)
	, Scene(InScene)
	, UVToWorld(InUVToWorld)
	, WorldBounds(InWorldBounds)
{
}

bool FRuntimeVirtualTextureFinalizer::IsReady()
{
	return RuntimeVirtualTexture::IsSceneReadyToRender(Scene);
}

void FRuntimeVirtualTextureFinalizer::InitProducer(const FVirtualTextureProducerHandle& ProducerHandle)
{
	if (RuntimeVirtualTextureMask == 0)
	{
		FScene* RenderScene = Scene->GetRenderScene();

		// Initialize the RuntimeVirtualTextureMask by matching this producer with those registered in the scene's runtime virtual textures.
		// We only need to do this once. If the associated scene proxy is removed this finalizer will also be destroyed.
		const uint32 VirtualTextureSceneIndex = RenderScene->GetRuntimeVirtualTextureSceneIndex(ProducerId);
		RuntimeVirtualTextureMask = 1 << VirtualTextureSceneIndex;

		//todo[vt]: 
		// Add a slow render path inside RenderPage() when this check fails. 
		// It will need to iterate the virtual textures on each primitive instead of using the RuntimeVirtualTextureMask.
		// Currently nothing will render for this finalizer when the check fails.
		checkSlow(VirtualTextureSceneIndex < FPrimitiveVirtualTextureFlags::RuntimeVirtualTexture_BitCount);
	}
}

void FRuntimeVirtualTextureFinalizer::AddTile(FTileEntry& Tile)
{
	Tiles.Add(Tile);
}

void FRuntimeVirtualTextureFinalizer::Finalize(FRDGBuilder& GraphBuilder)
{
	RDG_EVENT_SCOPE(GraphBuilder, "RuntimeVirtualTextureFinalize");
	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

	RuntimeVirtualTexture::FRenderPageBatchDesc RenderPageBatchDesc;
	RenderPageBatchDesc.Scene = Scene->GetRenderScene();
	RenderPageBatchDesc.RuntimeVirtualTextureMask = RuntimeVirtualTextureMask;
	RenderPageBatchDesc.UVToWorld = UVToWorld;
	RenderPageBatchDesc.WorldBounds = WorldBounds;
	RenderPageBatchDesc.MaterialType = MaterialType;
	RenderPageBatchDesc.MaxLevel = Desc.MaxLevel;
	RenderPageBatchDesc.bClearTextures = bClearTextures;
	RenderPageBatchDesc.bIsThumbnails = false;
	RenderPageBatchDesc.FixedColor = FLinearColor::Transparent;
	
	for (int LayerIndex = 0; LayerIndex < RuntimeVirtualTexture::MaxTextureLayers; ++LayerIndex)
	{
		RenderPageBatchDesc.Targets[LayerIndex].Texture = Tiles[0].Targets[LayerIndex].TextureRHI != nullptr ? Tiles[0].Targets[LayerIndex].TextureRHI->GetTexture2D() : nullptr;
		RenderPageBatchDesc.Targets[LayerIndex].PooledRenderTarget = Tiles[0].Targets[LayerIndex].PooledRenderTarget;
	}

	int32 BatchSize = 0;
	for (auto Entry : Tiles)
	{
		RuntimeVirtualTexture::FRenderPageDesc& RenderPageDesc = RenderPageBatchDesc.PageDescs[BatchSize];

		const float X = (float)FMath::ReverseMortonCode2_64(Entry.vAddress);
		const float Y = (float)FMath::ReverseMortonCode2_64(Entry.vAddress >> 1);
		const float DivisorX = (float)Desc.BlockWidthInTiles / (float)(1 << Entry.vLevel);
		const float DivisorY = (float)Desc.BlockHeightInTiles / (float)(1 << Entry.vLevel);

		const FVector2D UV(X / DivisorX, Y / DivisorY);
		const FVector2D UVSize(1.f / DivisorX, 1.f / DivisorY);
		const FVector2D UVBorder = UVSize * ((float)Desc.TileBorderSize / (float)Desc.TileSize);
		const FBox2D UVRange(UV - UVBorder, UV + UVSize + UVBorder);

		RenderPageDesc.vLevel = Entry.vLevel;
		RenderPageDesc.UVRange = UVRange;

		const int32 TileSize = Desc.TileSize + 2 * Desc.TileBorderSize;
		for (int LayerIndex = 0; LayerIndex < RuntimeVirtualTexture::MaxTextureLayers; ++LayerIndex)
		{
			const FVector2D DestinationBoxStart0(Entry.Targets[LayerIndex].pPageLocation.X * TileSize, Entry.Targets[LayerIndex].pPageLocation.Y * TileSize);
			RenderPageDesc.DestBox[LayerIndex] = FBox2D(DestinationBoxStart0, DestinationBoxStart0 + FVector2D(TileSize, TileSize));
		}

		bool bBreakBatchForTextures = false;
		for (int LayerIndex = 0; LayerIndex < RuntimeVirtualTexture::MaxTextureLayers; ++LayerIndex)
		{
			// This should never happen which is why we don't bother sorting to maximize batch size
			bBreakBatchForTextures |= (RenderPageBatchDesc.Targets[LayerIndex].Texture != Entry.Targets[LayerIndex].TextureRHI);
		}

		if (++BatchSize == RuntimeVirtualTexture::EMaxRenderPageBatch || bBreakBatchForTextures)
		{
			RenderPageBatchDesc.NumPageDescs = BatchSize;
			RuntimeVirtualTexture::RenderPages(GraphBuilder, RenderPageBatchDesc);
			BatchSize = 0;
		}

		if (bBreakBatchForTextures)
		{
			for (int LayerIndex = 0; LayerIndex < RuntimeVirtualTexture::MaxTextureLayers; ++LayerIndex)
			{
				RenderPageBatchDesc.Targets[LayerIndex].Texture = Tiles[0].Targets[LayerIndex].TextureRHI != nullptr ? Tiles[0].Targets[LayerIndex].TextureRHI->GetTexture2D() : nullptr;
				RenderPageBatchDesc.Targets[LayerIndex].PooledRenderTarget = Tiles[0].Targets[LayerIndex].PooledRenderTarget;
			}
		}
	}

	if (BatchSize > 0)
	{
		RenderPageBatchDesc.NumPageDescs = BatchSize;
		RuntimeVirtualTexture::RenderPages(GraphBuilder, RenderPageBatchDesc);
	}

	Tiles.SetNumUnsafeInternal(0);
}

FRuntimeVirtualTextureProducer::FRuntimeVirtualTextureProducer(
	FVTProducerDescription const& InDesc, 
	uint32 InProducerId, 
	ERuntimeVirtualTextureMaterialType InMaterialType, 
	bool InClearTextures, 
	FSceneInterface* InScene, 
	FTransform const& InUVToWorld,
	FBox const& InWorldBounds)
	: Finalizer(InDesc, InProducerId, InMaterialType, InClearTextures, InScene, InUVToWorld, InWorldBounds)
{
}

FVTRequestPageResult FRuntimeVirtualTextureProducer::RequestPageData(
	FRHICommandList& RHICmdList,
	const FVirtualTextureProducerHandle& ProducerHandle,
	uint8 LayerMask,
	uint8 vLevel,
	uint64 vAddress,
	EVTRequestPagePriority Priority)
{
	//todo[vt]: 
	// Possibly throttle rendering according to performance and return Saturated here.

	FVTRequestPageResult result;
	result.Handle = 0;
	result.Status = Finalizer.IsReady() ? EVTRequestPageStatus::Available : EVTRequestPageStatus::Pending;
	return result;
}

IVirtualTextureFinalizer* FRuntimeVirtualTextureProducer::ProducePageData(
	FRHICommandList& RHICmdList,
	ERHIFeatureLevel::Type FeatureLevel,
	EVTProducePageFlags Flags,
	const FVirtualTextureProducerHandle& ProducerHandle,
	uint8 LayerMask,
	uint8 vLevel,
	uint64 vAddress,
	uint64 RequestHandle,
	const FVTProduceTargetLayer* TargetLayers)
{
	FRuntimeVirtualTextureFinalizer::FTileEntry Tile;
	Tile.vAddress = vAddress;
	Tile.vLevel = vLevel;

	// Partial layer masks can happen when one layer has more physical space available so that old pages are evicted at different rates.
	// We currently render all layers even for these partial requests. That might be considered inefficient?
	// But since the problem is avoided by setting bSinglePhysicalSpace on the URuntimeVirtualTexture we can live with it.

	for (int LayerIndex = 0; LayerIndex < RuntimeVirtualTexture::MaxTextureLayers; ++LayerIndex)
	{
		if (TargetLayers[LayerIndex].TextureRHI != nullptr)
		{
			Tile.Targets[LayerIndex] = TargetLayers[LayerIndex];
		}
	}

	Finalizer.InitProducer(ProducerHandle);
	Finalizer.AddTile(Tile);

	return &Finalizer;
}

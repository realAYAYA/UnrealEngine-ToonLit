// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeVirtualTextureBuildStreamingMips.h"

#include "Components/RuntimeVirtualTextureComponent.h"
#include "ContentStreaming.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/ScopedSlowTask.h"
#include "RendererInterface.h"
#include "RenderTargetPool.h"
#include "SceneInterface.h"
#include "RenderGraphBuilder.h"
#include "VT/RuntimeVirtualTexture.h"
#include "VT/RuntimeVirtualTextureRender.h"
#include "VT/VirtualTextureBuilder.h"
#include "SceneUtils.h"

namespace
{
	/** Container for render resources needed to render the runtime virtual texture. */
	class FTileRenderResources : public FRenderResource
	{
	public:
		FTileRenderResources(int32 InTileSize, int32 InNumTilesX, int32 InNumTilesY, int32 InNumLayers, TArrayView<EPixelFormat> const& InLayerFormats)
			: TileSize(InTileSize)
			, NumLayers(InNumLayers)
			, TotalSizeBytes(0)
		{
			LayerFormats.SetNumZeroed(InNumLayers);
			LayerOffsets.SetNumZeroed(InNumLayers);

			for (int32 Layer = 0; Layer < NumLayers; ++Layer)
			{
				check(InLayerFormats[Layer] == PF_G16 || InLayerFormats[Layer] == PF_B8G8R8A8 || InLayerFormats[Layer] == PF_DXT1 || InLayerFormats[Layer] == PF_DXT5 || InLayerFormats[Layer] == PF_BC4
					|| InLayerFormats[Layer] == PF_BC5 || InLayerFormats[Layer] == PF_R5G6B5_UNORM || InLayerFormats[Layer] == PF_B5G5R5A1_UNORM);
				LayerFormats[Layer] = InLayerFormats[Layer] == PF_G16 || InLayerFormats[Layer] == PF_BC4 ? PF_G16 : PF_B8G8R8A8;
				LayerOffsets[Layer] = TotalSizeBytes;
				TotalSizeBytes += CalculateImageBytes(InTileSize, InTileSize, 0, LayerFormats[Layer]) * InNumTilesX * InNumTilesY;
			}
		}

		//~ Begin FRenderResource Interface.
		virtual void InitRHI(FRHICommandListBase& RHICmdList) override
		{
			RenderTargets.Init(nullptr, NumLayers);
			StagingTextures.Init(nullptr, NumLayers);

			for (int32 Layer = 0; Layer < NumLayers; ++Layer)
			{
				FRHITextureCreateDesc Desc =
					FRHITextureCreateDesc::Create2D(TEXT("FTileRenderResources"), TileSize, TileSize, LayerFormats[Layer]);

				Desc.SetFlags(ETextureCreateFlags::RenderTargetable);
				RenderTargets[Layer] = RHICreateTexture(Desc);

				Desc.SetFlags(ETextureCreateFlags::CPUReadback);
				StagingTextures[Layer] = RHICreateTexture(Desc);
			}

			Fence = RHICreateGPUFence(TEXT("Runtime Virtual Texture Build"));
		}

		virtual void ReleaseRHI() override
		{
			RenderTargets.Empty();
			StagingTextures.Empty();
			Fence.SafeRelease();
		}
		//~ End FRenderResource Interface.

		int32 GetNumLayers() const { return NumLayers; }
		int64 GetTotalSizeBytes() const { return TotalSizeBytes; }

		EPixelFormat GetLayerFormat(int32 Index) const { return LayerFormats[Index]; }
		int64 GetLayerOffset(int32 Index) const { return LayerOffsets[Index]; }

		FRHITexture2D* GetRenderTarget(int32 Index) const { return Index < NumLayers ? RenderTargets[Index] : nullptr; }
		FRHITexture2D* GetStagingTexture(int32 Index) const { return Index < NumLayers ? StagingTextures[Index] : nullptr; }
		FRHIGPUFence* GetFence() const { return Fence; }

	private:
		int32 TileSize;
		int32 NumLayers;
		int64 TotalSizeBytes;

		TArray<EPixelFormat> LayerFormats;
		TArray<int64> LayerOffsets;

		TArray<FTexture2DRHIRef> RenderTargets;
		TArray<FTexture2DRHIRef> StagingTextures;
		FGPUFenceRHIRef Fence;
	};

	/** Templatized helper function for copying a rendered tile to the final composited image data. */
	template<typename T>
	void TCopyTile(T* SrcPixels, int32 TileSize, int32 SrcStride, T* DestPixels, int32 DestStride, int32 DestLayerStride, FIntPoint const& DestPos)
	{
		for (int32 y = 0; y < TileSize; y++)
		{
			memcpy(
				DestPixels + (SIZE_T)DestStride * ((SIZE_T)DestPos[1] + (SIZE_T)y) + DestPos[0],
				SrcPixels + SrcStride * y,
				TileSize * sizeof(T));
		}
	}

	/** Function for copying a rendered tile to the final composited image data. Needs ERuntimeVirtualTextureMaterialType to know what type of data is being copied. */
	void CopyTile(void* SrcPixels, int32 TileSize, int32 SrcStride, void* DestPixels, int32 DestStride, int32 DestLayerStride, FIntPoint const& DestPos, EPixelFormat Format)
	{
		check(Format == PF_G16 || Format == PF_B8G8R8A8);
		if (Format == PF_G16)
		{
			TCopyTile((uint16*)SrcPixels, TileSize, SrcStride, (uint16*)DestPixels, DestStride, DestLayerStride, DestPos);
		}
		else if (Format == PF_B8G8R8A8)
		{
			TCopyTile((FColor*)SrcPixels, TileSize, SrcStride, (FColor*)DestPixels, DestStride, DestLayerStride, DestPos);
		}
	}
}


namespace RuntimeVirtualTexture
{
	bool HasStreamedMips(URuntimeVirtualTextureComponent* InComponent)
	{
		EShadingPath ShadingPath = (InComponent && InComponent->GetScene()) ? InComponent->GetScene()->GetShadingPath() : EShadingPath::Deferred;
		return HasStreamedMips(ShadingPath, InComponent);
	}

	bool HasStreamedMips(EShadingPath ShadingPath, URuntimeVirtualTextureComponent* InComponent)
	{
		if (InComponent == nullptr)
		{
			return false;
		}

		if (InComponent->GetVirtualTexture() == nullptr || InComponent->GetStreamingTexture() == nullptr)
		{
			return false;
		}

		if (InComponent->NumStreamingMips() <= 0)
		{
			return false;
		}

		if (ShadingPath == EShadingPath::Mobile && !InComponent->GetStreamingTexture()->bSeparateTextureForMobile)
		{
			return false;
		}

		return true;
	}
	
	bool BuildStreamedMips(URuntimeVirtualTextureComponent* InComponent, FLinearColor const& FixedColor)
	{
		EShadingPath ShadingPath = (InComponent && InComponent->GetScene()) ? InComponent->GetScene()->GetShadingPath() : EShadingPath::Deferred;
		return BuildStreamedMips(ShadingPath, InComponent, FixedColor);
	}

	bool BuildStreamedMips(EShadingPath ShadingPath, URuntimeVirtualTextureComponent* InComponent, FLinearColor const& FixedColor)
	{
		if (!HasStreamedMips(ShadingPath, InComponent))
		{
			return true;
		}

		URuntimeVirtualTexture* RuntimeVirtualTexture = InComponent->GetVirtualTexture();
		FSceneInterface* Scene = InComponent->GetScene();
		const uint32 VirtualTextureSceneIndex = RuntimeVirtualTexture::GetRuntimeVirtualTextureSceneIndex_GameThread(InComponent);
		const FTransform Transform = InComponent->GetComponentTransform();
		const FBox Bounds = InComponent->Bounds.GetBox();

		FVTProducerDescription VTDesc;
		RuntimeVirtualTexture->GetProducerDescription(VTDesc, URuntimeVirtualTexture::FInitSettings(), Transform);

		const int32 TileSize = VTDesc.TileSize;
		const int32 TileBorderSize = VTDesc.TileBorderSize;
		const int32 TextureSizeX = VTDesc.WidthInBlocks * VTDesc.BlockWidthInTiles * TileSize;
		const int32 TextureSizeY = VTDesc.HeightInBlocks * VTDesc.BlockHeightInTiles * TileSize;
		const int32 MaxLevel = (int32)FMath::CeilLogTwo(FMath::Max(VTDesc.BlockWidthInTiles, VTDesc.BlockHeightInTiles));
		const int32 RenderLevel = FMath::Max(MaxLevel - InComponent->NumStreamingMips() + 1, 0);
		const int32 ImageSizeX = FMath::Max(TileSize, TextureSizeX >> RenderLevel);
		const int32 ImageSizeY = FMath::Max(TileSize, TextureSizeY >> RenderLevel);
		const int32 NumTilesX = ImageSizeX / TileSize;
		const int32 NumTilesY = ImageSizeY / TileSize;
		const int32 NumLayers = RuntimeVirtualTexture->GetLayerCount();

		const ERuntimeVirtualTextureMaterialType MaterialType = RuntimeVirtualTexture->GetMaterialType();
		TArray<EPixelFormat, TInlineAllocator<4>> LayerFormats;
		for (int32 Layer = 0; Layer < NumLayers; ++Layer)
		{
			LayerFormats.Add(RuntimeVirtualTexture->GetLayerFormat(Layer));
		}

		// Spin up slow task UI
		const float TaskWorkRender = NumTilesX * NumTilesY;
		const float TextureBuildTaskMultiplier = 0.25f;
		const float TaskWorkBuildBulkData = TaskWorkRender * TextureBuildTaskMultiplier;
		FScopedSlowTask Task(TaskWorkRender + TaskWorkBuildBulkData, FText::AsCultureInvariant(InComponent->GetStreamingTexture()->GetName()));
		Task.MakeDialog(true);

		// Allocate render targets for rendering out the runtime virtual texture tiles
		FTileRenderResources RenderTileResources(TileSize, NumTilesX, NumTilesY, NumLayers, LayerFormats);
		BeginInitResource(&RenderTileResources);

		int64 RenderTileResourcesBytes = RenderTileResources.GetTotalSizeBytes();

		UE_LOG(LogVirtualTexturing,Display,TEXT("Allocating %uMiB for RenderTileResourcesBytes"),(uint32)(RenderTileResourcesBytes/(1024*1024)));

		// Final pixels will contain image data for each virtual texture layer in order
		TArray64<uint8> FinalPixels;
		FinalPixels.SetNumUninitialized(RenderTileResourcesBytes);

		UE::RenderCommandPipe::FSyncScope SyncScope;

		// Iterate over all tiles and render/store each one to the final image
		for (int32 TileY = 0; TileY < NumTilesY && !Task.ShouldCancel(); TileY++)
		{
			for (int32 TileX = 0; TileX < NumTilesX; TileX++)
			{
				// Render tile
				Task.EnterProgressFrame();

				const FBox2D UVRange = FBox2D(
					FVector2D((float)TileX / (float)NumTilesX, (float)TileY / (float)NumTilesY),
					FVector2D((float)(TileX + 1) / (float)NumTilesX, (float)(TileY + 1) / (float)NumTilesY));

				// Stream textures for this tile. This triggers a render flush internally.
				//todo[vt]: Batch groups of streaming locations and render commands to reduce number of flushes.
				const FVector StreamingWorldPos = Transform.TransformPosition(FVector(UVRange.GetCenter(), 0.5f));
				IStreamingManager::Get().Tick(0.f);
				IStreamingManager::Get().AddViewLocation(StreamingWorldPos);
				IStreamingManager::Get().StreamAllResources(0);

				ENQUEUE_RENDER_COMMAND(BakeStreamingTextureTileCommand)([
					Scene, VirtualTextureSceneIndex, 
					&RenderTileResources,
					MaterialType, NumLayers,
					Transform, Bounds, UVRange,
					RenderLevel, MaxLevel, 
					TileX, TileY,
					TileSize, ImageSizeX, ImageSizeY, 
					&FinalPixels,
					FixedColor](FRHICommandListImmediate& RHICmdList)
				{
					const FBox2D TileBox(FVector2D(0, 0), FVector2D(TileSize, TileSize));
					const FIntRect TileRect(0, 0, TileSize, TileSize);

					// Transition render targets for writing
					for (int32 Layer = 0; Layer < NumLayers; Layer++)
					{
						RHICmdList.Transition(FRHITransitionInfo(RenderTileResources.GetRenderTarget(Layer), ERHIAccess::Unknown, ERHIAccess::RTV));
					}

					{
						FRDGBuilder GraphBuilder(RHICmdList);

						RuntimeVirtualTexture::FRenderPageBatchDesc Desc;
						Desc.Scene = Scene->GetRenderScene();
						Desc.RuntimeVirtualTextureMask = 1 << VirtualTextureSceneIndex;
						Desc.UVToWorld = Transform;
						Desc.WorldBounds = Bounds;
						Desc.MaterialType = MaterialType;
						Desc.MaxLevel = MaxLevel;
						Desc.bClearTextures = true;
						Desc.bIsThumbnails = false;
						Desc.FixedColor = FixedColor;
						Desc.NumPageDescs = 1;
						Desc.Targets[0].Texture = RenderTileResources.GetRenderTarget(0);
						Desc.Targets[1].Texture = RenderTileResources.GetRenderTarget(1);
						Desc.Targets[2].Texture = RenderTileResources.GetRenderTarget(2);
						Desc.PageDescs[0].DestBox[0] = TileBox;
						Desc.PageDescs[0].DestBox[1] = TileBox;
						Desc.PageDescs[0].DestBox[2] = TileBox;
						Desc.PageDescs[0].UVRange = UVRange;
						Desc.PageDescs[0].vLevel = RenderLevel;

						RuntimeVirtualTexture::RenderPagesStandAlone(GraphBuilder, Desc);

						GraphBuilder.Execute();
					}

					// Copy to staging
					for (int32 Layer = 0; Layer < NumLayers; Layer++)
					{
						RHICmdList.Transition(FRHITransitionInfo(RenderTileResources.GetRenderTarget(Layer), ERHIAccess::RTV, ERHIAccess::CopySrc));
						RHICmdList.CopyTexture(RenderTileResources.GetRenderTarget(Layer), RenderTileResources.GetStagingTexture(Layer), FRHICopyTextureInfo());
					}

					RenderTileResources.GetFence()->Clear();
					RHICmdList.WriteGPUFence(RenderTileResources.GetFence());

					// Read back tile data and copy into final destination
					for (int32 Layer = 0; Layer < NumLayers; Layer++)
					{
						void* TilePixels = nullptr;
						int32 OutWidth, OutHeight;
						RHICmdList.MapStagingSurface(RenderTileResources.GetStagingTexture(Layer), RenderTileResources.GetFence(), TilePixels, OutWidth, OutHeight);
						check(TilePixels != nullptr);
						check(OutHeight == TileSize);

						const int64 LayerOffset = RenderTileResources.GetLayerOffset(Layer);
						const EPixelFormat LayerFormat = RenderTileResources.GetLayerFormat(Layer);
						const FIntPoint DestPos(TileX * TileSize, TileY * TileSize);

						CopyTile(TilePixels, TileSize, OutWidth, FinalPixels.GetData() + LayerOffset, ImageSizeX, ImageSizeX * ImageSizeY, DestPos, LayerFormat);

						RHICmdList.UnmapStagingSurface(RenderTileResources.GetStagingTexture(Layer));
					}
				});
			}
		}

		BeginReleaseResource(&RenderTileResources);
		FlushRenderingCommands();

		if (Task.ShouldCancel())
		{
			return false;
		}

		// Place final pixel data into the runtime virtual texture
		Task.EnterProgressFrame(TaskWorkBuildBulkData);

		InComponent->InitializeStreamingTexture(ShadingPath, ImageSizeX, ImageSizeY, (uint8*)FinalPixels.GetData());

		return true;
	}
}

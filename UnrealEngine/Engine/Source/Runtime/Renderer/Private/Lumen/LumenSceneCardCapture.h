// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MeshCardRepresentation.h"
#include "SceneView.h"
#include "MeshPassProcessor.h"

class FScene;
class FLumenPrimitiveGroup;
class FNaniteCommandInfo;
class FLumenCard;

struct FCardCaptureAtlas
{
	FIntPoint Size;
	FRDGTextureRef Albedo;
	FRDGTextureRef Normal;
	FRDGTextureRef Emissive;
	FRDGTextureRef DepthStencil;
};

struct FResampledCardCaptureAtlas
{
	FIntPoint Size;
	FRDGTextureRef DirectLighting = nullptr;
	FRDGTextureRef IndirectLighting = nullptr;
	FRDGTextureRef NumFramesAccumulated = nullptr;
};

class FCardPageRenderData
{
public:
	int32 PrimitiveGroupIndex = -1;

	// CardData
	const int32 CardIndex = -1;
	const int32 PageTableIndex = -1;
	FVector4f CardUVRect;
	FIntRect CardCaptureAtlasRect;
	FIntRect SurfaceCacheAtlasRect;

	FLumenCardOBBd CardWorldOBB;

	FViewMatrices ViewMatrices;
	FMatrix ProjectionMatrixUnadjustedForRHI;

	int32 StartMeshDrawCommandIndex = 0;
	int32 NumMeshDrawCommands = 0;

	TArray<uint32, SceneRenderingAllocator> NaniteInstanceIds;
	TArray<FNaniteCommandInfo, SceneRenderingAllocator> NaniteCommandInfos;
	float NaniteLODScaleFactor = 1.0f;

	bool bResampleLastLighting = false;

	// Non-Nanite mesh inclusive instance ranges to draw
	TArray<uint32, SceneRenderingAllocator> InstanceRuns;

	FCardPageRenderData(
		const FViewInfo& InMainView,
		const FLumenCard& InLumenCard,
		FVector4f InCardUVRect,
		FIntRect InCardCaptureAtlasRect,
		FIntRect InSurfaceCacheAtlasRect,
		int32 InPrimitiveGroupIndex,
		int32 InCardIndex,
		int32 InCardPageIndex,
		bool bResampleLastLighting);
	~FCardPageRenderData();

	void UpdateViewMatrices(const FViewInfo& MainView);

	void PatchView(const FScene* Scene, FViewInfo* View) const;
};

namespace LumenScene
{
	bool HasPrimitiveNaniteMeshBatches(const FPrimitiveSceneProxy* Proxy);

	void AllocateCardCaptureAtlas(FRDGBuilder& GraphBuilder, FIntPoint CardCaptureAtlasSize, FCardCaptureAtlas& CardCaptureAtlas);

	void AddCardCaptureDraws(
		const FScene* Scene,
		FCardPageRenderData& CardPageRenderData,
		const FLumenPrimitiveGroup& PrimitiveGroup,
		TConstArrayView<const FPrimitiveSceneInfo*> SceneInfoPrimitives,
		FMeshCommandOneFrameArray& VisibleMeshCommands,
		TArray<int32, SceneRenderingAllocator>& PrimitiveIds);
};

FMeshPassProcessor* CreateLumenCardNaniteMeshProcessor(
	ERHIFeatureLevel::Type FeatureLevel,
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	FMeshPassDrawListContext* InDrawListContext);
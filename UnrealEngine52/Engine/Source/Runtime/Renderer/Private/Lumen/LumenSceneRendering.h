// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenSceneRendering.h
=============================================================================*/

#pragma once

#include "CoreTypes.h"
#include "MeshCardRepresentation.h"
#include "SceneView.h"
#include "ShaderParameterMacros.h"

enum EShaderPlatform : uint16;

class FDistanceFieldSceneData;
class FLumenSceneData;
class FLumenCard;
class FLumenCardScene;
class FMeshPassDrawListContext;
class FMeshPassProcessor;
class FNaniteCommandInfo;
class FPrimitiveSceneProxy;
class FScene;
class FViewFamilyInfo;
class FViewInfo;

struct FLumenSceneFrameTemporaries;

bool DoesPlatformSupportLumenGI(EShaderPlatform Platform, bool bSkipProjectCheck = false);

class FCardPageRenderData
{
public:
	int32 PrimitiveGroupIndex = -1;

	// CardData
	const int32 CardIndex = -1;
	const int32 PageTableIndex = -1;
	bool bDistantScene = false;
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

	FCardPageRenderData(const FViewInfo& InMainView,
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

struct FResampledCardCaptureAtlas
{
	FIntPoint Size;
	FRDGTextureRef DirectLighting = nullptr;
	FRDGTextureRef IndirectLighting = nullptr;
	FRDGTextureRef NumFramesAccumulated = nullptr;
};

FMeshPassProcessor* CreateLumenCardNaniteMeshProcessor(
	ERHIFeatureLevel::Type FeatureLevel,
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	FMeshPassDrawListContext* InDrawListContext);

namespace Lumen
{
	bool HasPrimitiveNaniteMeshBatches(const FPrimitiveSceneProxy* Proxy);
};

extern void UpdateLumenCardSceneUniformBuffer(FRDGBuilder& GraphBuilder, FScene* Scene, const FLumenSceneData& LumenSceneData, FLumenSceneFrameTemporaries& FrameTemporaries);
extern void UpdateLumenMeshCards(FRDGBuilder& GraphBuilder, const FScene& Scene, const FDistanceFieldSceneData& DistanceFieldSceneData, FLumenSceneFrameTemporaries& FrameTemporaries, FLumenSceneData& LumenSceneData);

namespace LumenDiffuseIndirect
{
	bool UseAsyncCompute(const FViewFamilyInfo& ViewFamily);
}

namespace LumenReflections
{
	BEGIN_SHADER_PARAMETER_STRUCT(FCompositeParameters, )
		SHADER_PARAMETER(float, MaxRoughnessToTrace)
		SHADER_PARAMETER(float, MaxRoughnessToTraceForFoliage)
		SHADER_PARAMETER(float, InvRoughnessFadeLength)
	END_SHADER_PARAMETER_STRUCT()

	void SetupCompositeParameters(LumenReflections::FCompositeParameters& OutParameters);
	bool UseAsyncCompute(const FViewFamilyInfo& ViewFamily);
}

BEGIN_SHADER_PARAMETER_STRUCT(FLumenScreenSpaceBentNormalParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, ScreenBentNormal)
	SHADER_PARAMETER(uint32, UseShortRangeAO)
END_SHADER_PARAMETER_STRUCT()
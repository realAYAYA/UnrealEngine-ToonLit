// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenSceneRendering.h
=============================================================================*/

#pragma once

#include "RHIDefinitions.h"
#include "SceneView.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "LumenSceneData.h"

class FLumenSceneData;
class FLumenCardScene;

inline bool DoesPlatformSupportLumenGI(EShaderPlatform Platform, bool bSkipProjectCheck = false)
{
	extern int32 GLumenSupported;
	return (bSkipProjectCheck || GLumenSupported)
		&& FDataDrivenShaderPlatformInfo::GetSupportsLumenGI(Platform)
		&& !IsForwardShadingEnabled(Platform);
}

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

	FLumenCardOBB CardWorldOBB;

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
	inline bool HasPrimitiveNaniteMeshBatches(const FPrimitiveSceneProxy* Proxy)
	{
		return Proxy && Proxy->ShouldRenderInMainPass() && Proxy->AffectsDynamicIndirectLighting();
	}
};

extern void UpdateLumenCardSceneUniformBuffer(FRDGBuilder& GraphBuilder, FScene* Scene, const FLumenSceneData& LumenSceneData, FLumenSceneFrameTemporaries& FrameTemporaries);
extern void UpdateLumenMeshCards(FRDGBuilder& GraphBuilder, const FScene& Scene, const FDistanceFieldSceneData& DistanceFieldSceneData, FLumenSceneFrameTemporaries& FrameTemporaries, FLumenSceneData& LumenSceneData);

BEGIN_SHADER_PARAMETER_STRUCT(FLumenReflectionCompositeParameters, )
	SHADER_PARAMETER(float, MaxRoughnessToTrace)
	SHADER_PARAMETER(float, InvRoughnessFadeLength)
END_SHADER_PARAMETER_STRUCT()

extern FLumenReflectionCompositeParameters GetLumenReflectionCompositeParameters();

BEGIN_SHADER_PARAMETER_STRUCT(FLumenScreenSpaceBentNormalParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, ScreenBentNormal)
	SHADER_PARAMETER(uint32, UseScreenBentNormal)
END_SHADER_PARAMETER_STRUCT()
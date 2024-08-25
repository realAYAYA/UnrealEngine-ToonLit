// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "DecalRenderingCommon.h"

class FDeferredDecalProxy;
class FMaterial;
class FMaterialRenderProxy;
class FScene;
class FViewInfo;

/**
 * Compact deferred decal data for rendering.
 */
struct FTransientDecalRenderData
{
	const FDeferredDecalProxy* Proxy;
	const FMaterialRenderProxy* MaterialProxy;
	FDecalBlendDesc BlendDesc;
	float ConservativeRadius;
	float FadeAlpha;
	FLinearColor DecalColor;

	FTransientDecalRenderData() = default; // required to support TChunkedArray
	FTransientDecalRenderData(const FDeferredDecalProxy& InDecalProxy, float InConservativeRadius, float InFadeAlpha, EShaderPlatform ShaderPlatform, ERHIFeatureLevel::Type FeatureLevel);
};
	
typedef TArray<FTransientDecalRenderData, SceneRenderingAllocator> FTransientDecalRenderDataList;

/**
 * Shared deferred decal functionality.
 */
namespace DecalRendering
{
	float GetDecalFadeScreenSizeMultiplier();
	float CalculateDecalFadeAlpha(float DecalFadeScreenSize, const FMatrix& ComponentToWorldMatrix, const FViewInfo& View, float FadeMultiplier);
	FMatrix ComputeComponentToClipMatrix(const FViewInfo& View, const FMatrix& DecalComponentToWorld);
	void SetVertexShaderOnly(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View, const FMatrix& FrustumComponentToClip);
	void SortDecalList(FTransientDecalRenderDataList& Decals);
	FTransientDecalRenderDataList BuildVisibleDecalList(TConstArrayView<FDeferredDecalProxy*> Decals, const FViewInfo& View);
	bool BuildRelevantDecalList(TConstArrayView<FTransientDecalRenderData> Decals, EDecalRenderStage DecalRenderStage, FTransientDecalRenderDataList* OutVisibleDecals);
	bool SetupShaderState(ERHIFeatureLevel::Type FeatureLevel, const FMaterial& Material, EDecalRenderStage DecalRenderStage, FBoundShaderStateInput& OutBoundShaderState);
	void SetShader(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, uint32 StencilRef, const FViewInfo& View, const FTransientDecalRenderData& DecalData, EDecalRenderStage DecalRenderStage, const FMatrix& FrustumComponentToClip);
};

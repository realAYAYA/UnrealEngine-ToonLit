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
	const FDeferredDecalProxy& Proxy;
	const FMaterialRenderProxy* MaterialProxy;
	FDecalBlendDesc BlendDesc;
	float ConservativeRadius;
	float FadeAlpha;

	FTransientDecalRenderData(const FScene& InScene, const FDeferredDecalProxy& InDecalProxy, float InConservativeRadius);
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
	bool BuildVisibleDecalList(const FScene& Scene, const FViewInfo& View, EDecalRenderStage DecalRenderStage, FTransientDecalRenderDataList* OutVisibleDecals);
	void SetShader(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, uint32 StencilRef, const FViewInfo& View, const FTransientDecalRenderData& DecalData, EDecalRenderStage DecalRenderStage, const FMatrix& FrustumComponentToClip);
};

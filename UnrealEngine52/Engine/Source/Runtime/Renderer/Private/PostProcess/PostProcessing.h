// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraphFwd.h"
#include "RHIFwd.h"
#include "TranslucentRendering.h"

enum class EReflectionsMethod;

class FSceneTextureParameters;
class FSceneTextureUniformParameters;
class FScreenPassVS;
class FViewInfo;
class FVirtualShadowMapArray;

namespace Nanite
{
	struct FRasterResults;
}

// Returns whether the full post process pipeline is enabled. Otherwise, the minimal set of operations are performed.
bool IsPostProcessingEnabled(const FViewInfo& View);

// Returns whether the post process pipeline supports using compute passes.
bool IsPostProcessingWithComputeEnabled(ERHIFeatureLevel::Type FeatureLevel);

// Returns whether the post process pipeline supports propagating the alpha channel.
bool IsPostProcessingWithAlphaChannelSupported();

using FPostProcessVS = FScreenPassVS;

struct FPostProcessingInputs
{
	TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures = nullptr;
	FRDGTextureRef ViewFamilyTexture = nullptr;
	FRDGTextureRef CustomDepthTexture = nullptr;
	FRDGTextureRef ExposureIlluminance = nullptr;
	FTranslucencyViewResourcesMap TranslucencyViewResourcesMap;
	bool bSeparateCustomStencil = false;

	void Validate() const
	{
		check(SceneTextures);
		check(ViewFamilyTexture);
		check(TranslucencyViewResourcesMap.IsValid());
	}
};

void AddPostProcessingPasses(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View, int32 ViewIndex,
	bool bAnyLumenActive,
	EReflectionsMethod ReflectionsMethod,
	const FPostProcessingInputs& Inputs,
	const Nanite::FRasterResults* NaniteRasterResults,
	FInstanceCullingManager& InstanceCullingManager,
	FVirtualShadowMapArray* VirtualShadowMapArray,
	struct FLumenSceneFrameTemporaries& LumenFrameTemporaries,
	const FSceneWithoutWaterTextures& SceneWithoutWaterTextures,
	FScreenPassTexture TSRMoireInput);

void AddDebugViewPostProcessingPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FPostProcessingInputs& Inputs, const Nanite::FRasterResults* NaniteRasterResults);

void AddVisualizeCalibrationMaterialPostProcessingPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FPostProcessingInputs& Inputs, const UMaterialInterface* InMaterialInterface);

struct FMobilePostProcessingInputs
{
	TRDGUniformBufferRef<FMobileSceneTextureUniformParameters> SceneTextures = nullptr;
	FRDGTextureRef ViewFamilyTexture = nullptr;
	bool bRequiresMultiPass = false;

	void Validate() const
	{
		check(ViewFamilyTexture);
		check(SceneTextures);
	}
};

void AddMobilePostProcessingPasses(FRDGBuilder& GraphBuilder, FScene* Scene, const FViewInfo& View, const FMobilePostProcessingInputs& Inputs, FInstanceCullingManager& InstanceCullingManager);

void AddBasicPostProcessPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View);

FRDGTextureRef AddProcessPlanarReflectionPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef SceneColorTexture);

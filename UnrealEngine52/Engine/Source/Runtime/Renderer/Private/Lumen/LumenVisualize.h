// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraphFwd.h"
#include "ScreenPass.h"
#include "ShaderParameterMacros.h"
#include "Math/IntPoint.h"

class FScene;
class FSceneTextureParameters;
class FViewInfo;
class FLumenCardTracingInputs;
class FLumenIndirectTracingParameters;

struct FLumenSceneFrameTemporaries;

// r.Lumen.Visualize.Mode
#define VISUALIZE_MODE_LUMEN_SCENE 1
#define VISUALIZE_MODE_REFLECTION_VIEW 2
#define VISUALIZE_MODE_SURFACE_CACHE 3
#define VISUALIZE_MODE_OVERVIEW 4

BEGIN_SHADER_PARAMETER_STRUCT(FLumenVisualizeSceneParameters, )
	SHADER_PARAMETER(FIntPoint, InputViewSize)
	SHADER_PARAMETER(FIntPoint, InputViewOffset)
	SHADER_PARAMETER(FIntPoint, OutputViewSize)
	SHADER_PARAMETER(FIntPoint, OutputViewOffset)
	SHADER_PARAMETER(int32, VisualizeHiResSurface)
	SHADER_PARAMETER(int32, Tonemap)
	SHADER_PARAMETER(int32, VisualizeMode)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EyeAdaptationBuffer)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D, ColorGradingLUT)
	SHADER_PARAMETER_SAMPLER(SamplerState, ColorGradingLUTSampler)
END_SHADER_PARAMETER_STRUCT()

namespace LumenVisualize
{
	constexpr int32 NumOverviewTilesPerRow = 3;
	constexpr int32 OverviewTileMargin = 4;

	void VisualizeHardwareRayTracing(
		FRDGBuilder& GraphBuilder,
		const FScene* Scene,
		const FSceneTextureParameters& SceneTextures,
		const FViewInfo& View,
		const FLumenSceneFrameTemporaries& FrameTemporaries,
		const FLumenCardTracingParameters& TracingParameters,
		FLumenIndirectTracingParameters& IndirectTracingParameters,
		FLumenVisualizeSceneParameters& VisualizeParameters,
		FRDGTextureRef SceneColor,
		bool bVisualizeModeWithHitLighting);

	bool IsHitLightingForceEnabled(const FViewInfo& View);
	bool UseSurfaceCacheFeedback(const FEngineShowFlags& ShowFlags);
};

struct FVisualizeLumenSceneInputs
{
	// [Optional] Render to the specified output. If invalid, a new texture is created and returned.
	FScreenPassRenderTarget OverrideOutput;

	// [Required] The scene color
	FScreenPassTexture SceneColor;

	// [Required] The scene depth
	FScreenPassTexture SceneDepth;

	FRDGTextureRef ColorGradingTexture = nullptr;
	FRDGBufferRef EyeAdaptationBuffer = nullptr;

	// [Required] Used when scene textures are required by the material.
	FSceneTextureShaderParameters SceneTextures;
};

extern FScreenPassTexture AddVisualizeLumenScenePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, bool bAnyLumenActive, const FVisualizeLumenSceneInputs& Inputs, FLumenSceneFrameTemporaries& FrameTemporaries);

extern int32 GetLumenVisualizeMode(const FViewInfo& View);
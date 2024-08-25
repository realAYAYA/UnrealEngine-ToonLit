// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraphFwd.h"
#include "SceneTexturesConfig.h"
#include "ScreenPass.h"
#include "ShaderParameterMacros.h"
#include "Math/IntPoint.h"
#include "LumenReflections.h"

class FScene;
class FSceneTextureParameters;
class FViewInfo;
class FLumenCardTracingInputs;
class FLumenCardTracingParameters;
class FLumenIndirectTracingParameters;

struct FLumenSceneFrameTemporaries;

// r.Lumen.Visualize.Mode
#define VISUALIZE_MODE_OVERVIEW						1
#define VISUALIZE_MODE_PERFORMANCE_OVERVIEW			2
#define VISUALIZE_MODE_LUMEN_SCENE					3
#define VISUALIZE_MODE_REFLECTION_VIEW				4
#define VISUALIZE_MODE_SURFACE_CACHE				5
#define VISUALIZE_MODE_GEOMETRY_NORMALS				6
#define VISUALIZE_MODE_DEDICATED_REFLECTION_RAYS	7

namespace LumenVisualize
{
	BEGIN_SHADER_PARAMETER_STRUCT(FTonemappingParameters, )
		SHADER_PARAMETER(int32, Tonemap)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EyeAdaptationBuffer)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, ColorGradingLUT)
		SHADER_PARAMETER_SAMPLER(SamplerState, ColorGradingLUTSampler)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FSceneParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenVisualize::FTonemappingParameters, TonemappingParameters)
		SHADER_PARAMETER(FIntPoint, InputViewSize)
		SHADER_PARAMETER(FIntPoint, InputViewOffset)
		SHADER_PARAMETER(FIntPoint, OutputViewSize)
		SHADER_PARAMETER(FIntPoint, OutputViewOffset)
		SHADER_PARAMETER(int32, VisualizeHiResSurface)
		SHADER_PARAMETER(int32, VisualizeMode)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenReflections::FCompositeParameters, ReflectionsCompositeParameters)
		SHADER_PARAMETER_TEXTURE(Texture2D, PreIntegratedGF)
		SHADER_PARAMETER_SAMPLER(SamplerState, PreIntegratedGFSampler)
		SHADER_PARAMETER(uint32, MaxReflectionBounces)
		SHADER_PARAMETER(uint32, MaxRefractionBounces)
	END_SHADER_PARAMETER_STRUCT()

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
		LumenVisualize::FSceneParameters& VisualizeParameters,
		FRDGTextureRef SceneColor,
		bool bVisualizeModeWithHitLighting,
		bool bLumenGIEnabled);

	bool IsHitLightingForceEnabled(const FViewInfo& View, bool bLumenGIEnabled);
	bool UseHitLighting(const FViewInfo& View, bool bLumenGIEnabled);
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

extern FScreenPassTexture AddVisualizeLumenScenePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, bool bAnyLumenActive, bool bLumenGIEnabled, const FVisualizeLumenSceneInputs& Inputs, FLumenSceneFrameTemporaries& FrameTemporaries);

extern int32 GetLumenVisualizeMode(const FViewInfo& View);
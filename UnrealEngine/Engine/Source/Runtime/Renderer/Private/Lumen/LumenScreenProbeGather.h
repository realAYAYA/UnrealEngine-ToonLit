// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "BlueNoise.h"
#include "LumenTracingUtils.h"
#include "ShaderParameterMacros.h"

class FLumenScreenSpaceBentNormalParameters;
class FViewInfo;
struct FEngineShowFlags;
enum class ERDGPassFlags : uint16;

namespace LumenRadianceCache
{
	class FRadianceCacheInterpolationParameters;
	class FRadianceCacheMarkParameters;
}

extern int32 GLumenScreenProbeGatherNumMips;

// Must match LumenScreenProbeCommon.ush
enum class EScreenProbeIrradianceFormat : uint8
{
	SH3,
	Octahedral,

	MAX
};

namespace LumenScreenProbeGather 
{
	extern int32 GetTracingOctahedronResolution(const FViewInfo& View);
	extern int32 IsProbeTracingResolutionSupportedForImportanceSampling(int32 TracingResolution);
	extern bool UseImportanceSampling(const FViewInfo& View);
	extern bool UseProbeSpatialFilter();
	extern bool UseProbeTemporalFilter();
	extern bool UseRadianceCache(const FViewInfo& View);
	EScreenProbeIrradianceFormat GetScreenProbeIrradianceFormat(const FEngineShowFlags& ShowFlags);

	// Must match LumenScreenProbeCommon.ush
	constexpr uint32 IrradianceProbeRes = 6;
	constexpr uint32 IrradianceProbeWithBorderRes = (IrradianceProbeRes + 2);
}

// Must match SetupAdaptiveProbeIndirectArgsCS in usf
enum class EScreenProbeIndirectArgs
{
	GroupPerProbe,
	ThreadPerProbe,
	ThreadPerTrace,
	ThreadPerGather,
	ThreadPerGatherWithBorder,
	ThreadPerLightSample,
	Max
};

// Must match TILE_CLASSIFICATION_NUM in usf
enum class EScreenProbeIntegrateTileClassification
{
	SimpleDiffuse,
	SupportImportanceSampleBRDF,
	SupportAll,
	Num
};

BEGIN_SHADER_PARAMETER_STRUCT(FScreenProbeImportanceSamplingParameters, )
	SHADER_PARAMETER(uint32, MaxImportanceSamplingOctahedronResolution)
	SHADER_PARAMETER(uint32, ScreenProbeBRDFOctahedronResolution)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, StructuredImportanceSampledRayInfosForTracing)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FScreenProbeParameters, )
	SHADER_PARAMETER(uint32, ScreenProbeTracingOctahedronResolution)
	SHADER_PARAMETER(uint32, ScreenProbeLightSampleResolutionXY)
	SHADER_PARAMETER(uint32, ScreenProbeGatherOctahedronResolution)
	SHADER_PARAMETER(uint32, ScreenProbeGatherOctahedronResolutionWithBorder)
	SHADER_PARAMETER(uint32, ScreenProbeDownsampleFactor)
	SHADER_PARAMETER(FIntPoint, ScreenProbeViewSize)
	SHADER_PARAMETER(FIntPoint, ScreenProbeAtlasViewSize)
	SHADER_PARAMETER(FIntPoint, ScreenProbeAtlasBufferSize)
	SHADER_PARAMETER(float, ScreenProbeGatherMaxMip)
	SHADER_PARAMETER(float, RelativeSpeedDifferenceToConsiderLightingMoving)
	SHADER_PARAMETER(float, ScreenTraceNoFallbackThicknessScale)
	SHADER_PARAMETER(FVector2f, SampleRadianceProbeUVMul)
	SHADER_PARAMETER(FVector2f, SampleRadianceProbeUVAdd)
	SHADER_PARAMETER(FVector2f, SampleRadianceAtlasUVMul)
	SHADER_PARAMETER(uint32, AdaptiveScreenTileSampleResolution)
	SHADER_PARAMETER(uint32, NumUniformScreenProbes)
	SHADER_PARAMETER(uint32, MaxNumAdaptiveProbes)
	SHADER_PARAMETER(int32, FixedJitterIndex)
	SHADER_PARAMETER(uint32, ScreenProbeRayDirectionFrameIndex)

	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, NumAdaptiveScreenProbes)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, AdaptiveScreenProbeData)

	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenTileAdaptiveProbeHeader)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenTileAdaptiveProbeIndices)

	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TraceRadiance)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TraceHit)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenProbeSceneDepth)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenProbeWorldNormal)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenProbeWorldSpeed)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenProbeTranslatedWorldPosition)
	
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWTraceRadiance)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWTraceHit)

	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LightSampleTraceRadiance)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LightSampleTraceHit)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenProbeLightSampleDirection)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenProbeLightSampleFlags)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenProbeLightSampleRadiance)

	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWLightSampleTraceRadiance)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSampleTraceHit)

	SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeImportanceSamplingParameters, ImportanceSampling)
	SHADER_PARAMETER_STRUCT_REF(FBlueNoise, BlueNoise)

	RDG_BUFFER_ACCESS(ProbeIndirectArgs, ERHIAccess::IndirectArgs)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FScreenProbeGatherParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenProbeRadiance)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenProbeRadianceWithBorder)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, ScreenProbeRadianceSHAmbient)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, ScreenProbeRadianceSHDirectional)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, ScreenProbeIrradianceWithBorder)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, ScreenProbeMoving)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FCompactedTraceParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompactedTraceTexelAllocator)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompactedTraceTexelData)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompactedLightSampleTraceTexelData)
	RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
END_SHADER_PARAMETER_STRUCT()

namespace LumenScreenProbeGather
{
	FCompactedTraceParameters CompactTraces(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		const FScreenProbeParameters& ScreenProbeParameters,
		bool bCullByDistanceFromCamera,
		float CompactionTracingEndDistanceFromCamera,
		float CompactionMaxTraceDistance,
		bool bRenderDirectLighting,
		bool bCompactForFarField,
		ERDGPassFlags ComputePassFlags = ERDGPassFlags::Compute);
}

extern void GenerateBRDF_PDF(
	FRDGBuilder& GraphBuilder, 
	const FViewInfo& View,
	const FSceneTextures& SceneTextures,
	FRDGTextureRef& BRDFProbabilityDensityFunction,
	FRDGBufferSRVRef& BRDFProbabilityDensityFunctionSH,
	FScreenProbeParameters& ScreenProbeParameters,
	ERDGPassFlags ComputePassFlags);

extern void GenerateImportanceSamplingRays(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FSceneTextures& SceneTextures,
	const LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	FRDGTextureRef BRDFProbabilityDensityFunction,
	FRDGBufferSRVRef BRDFProbabilityDensityFunctionSH,
	FScreenProbeParameters& ScreenProbeParameters,
	ERDGPassFlags ComputePassFlags);

extern void TraceScreenProbes(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	bool bTraceMeshObjects,
	bool bRenderDirectLighting,
	const FSceneTextures& SceneTextures,
	FRDGTextureRef LightingChannelsTexture,
	const LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	FScreenProbeParameters& ScreenProbeParameters,
	FLumenMeshSDFGridParameters& MeshSDFGridParameters,
	ERDGPassFlags ComputePassFlags);

void RenderHardwareRayTracingScreenProbe(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FSceneTextureParameters& SceneTextures,
	FScreenProbeParameters& CommonDiffuseParameters,
	const FViewInfo& View,
	const FLumenCardTracingParameters& TracingParameters,
	FLumenIndirectTracingParameters& DiffuseTracingParameters,
	const LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	bool bRenderDirectLighting,
	ERDGPassFlags ComputePassFlags);

extern void RenderHardwareRayTracingShortRangeAO(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FSceneTextureParameters& SceneTextures,
	const FBlueNoise& BlueNoise,
	float MaxScreenTraceFraction,
	const FViewInfo& View,
	FRDGTextureRef ScreenBentNormal,
	uint32 NumPixelRays);

extern void FilterScreenProbes(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FSceneTextures& SceneTextures,
	const FScreenProbeParameters& ScreenProbeParameters,
	bool bRenderDirectLighting,
	FScreenProbeGatherParameters& GatherParameters,
	ERDGPassFlags ComputePassFlags);

extern FLumenScreenSpaceBentNormalParameters ComputeScreenSpaceShortRangeAO(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	const FSceneTextures& SceneTextures,
	FRDGTextureRef LightingChannelsTexture,
	const FBlueNoise& BlueNoise,
	float MaxScreenTraceFraction,
	float ScreenTraceNoFallbackThicknessScale,
	ERDGPassFlags ComputePassFlags);

namespace LumenScreenProbeGatherRadianceCache
{
	LumenRadianceCache::FRadianceCacheInputs SetupRadianceCacheInputs(const FViewInfo& View);
}

extern bool CanMaterialRenderInLumenTranslucencyRadianceCacheMarkPass(
	const FScene& Scene,
	const FSceneViewFamily& ViewFamily,
	const FPrimitiveSceneProxy& PrimitiveSceneProxy,
	const FMaterial& Material);

extern bool CanMaterialRenderInLumenFrontLayerTranslucencyGBufferPass(
	const FScene& Scene,
	const FSceneViewFamily& ViewFamily,
	const FPrimitiveSceneProxy& PrimitiveSceneProxy,
	const FMaterial& Material);

extern void LumenTranslucencyReflectionsMarkUsedProbes(
	FRDGBuilder& GraphBuilder,
	const FSceneRenderer& SceneRenderer,
	FViewInfo& View,
	const FSceneTextures& SceneTextures,
	const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters);
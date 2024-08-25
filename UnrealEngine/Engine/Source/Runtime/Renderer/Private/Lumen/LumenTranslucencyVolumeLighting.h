// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIDefinitions.h"
#include "RendererInterface.h"
#include "ShaderParameterMacros.h"
#include "LumenRadianceCacheInterpolation.h"
#include "LumenFrontLayerTranslucency.h"

class FSceneTextureParameters;
struct FLumenSceneFrameTemporaries;
class FLumenCardTracingParameters;

class FLumenTranslucencyGIVolume
{
public:
	LumenRadianceCache::FRadianceCacheInterpolationParameters RadianceCacheInterpolationParameters;
	FRDGTextureRef Texture0        = nullptr;
	FRDGTextureRef Texture1        = nullptr;
	FRDGTextureRef HistoryTexture0 = nullptr;
	FRDGTextureRef HistoryTexture1 = nullptr;
	FVector GridZParams            = FVector::ZeroVector;
	uint32 GridPixelSizeShift      = 0;
	FIntVector GridSize            = FIntVector::ZeroValue;
};

// Used by translucent BasePass
BEGIN_SHADER_PARAMETER_STRUCT(FLumenTranslucencyLightingParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheInterpolationParameters)
	SHADER_PARAMETER_STRUCT_INCLUDE(FLumenFrontLayerTranslucencyReflectionParameters, FrontLayerTranslucencyReflectionParameters)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D, TranslucencyGIVolume0)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D, TranslucencyGIVolume1)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D, TranslucencyGIVolumeHistory0)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D, TranslucencyGIVolumeHistory1)
	SHADER_PARAMETER_SAMPLER(SamplerState, TranslucencyGIVolumeSampler)
	SHADER_PARAMETER(FVector3f, TranslucencyGIGridZParams)
	SHADER_PARAMETER(uint32, TranslucencyGIGridPixelSizeShift)
	SHADER_PARAMETER(FIntVector, TranslucencyGIGridSize)
END_SHADER_PARAMETER_STRUCT()

// Used by VolumetricFog and HeterogeneousVolumes
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FLumenTranslucencyLightingUniforms, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingParameters, Parameters)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

extern FLumenTranslucencyLightingParameters GetLumenTranslucencyLightingParameters(
	FRDGBuilder& GraphBuilder, 
	const FLumenTranslucencyGIVolume& LumenTranslucencyGIVolume,
	const FLumenFrontLayerTranslucency& LumenFrontLayerTranslucency
);

// Used by Translucency Lighting pipeline shaders
BEGIN_SHADER_PARAMETER_STRUCT(FLumenTranslucencyLightingVolumeParameters, )
	SHADER_PARAMETER(FVector3f, TranslucencyGIGridZParams)
	SHADER_PARAMETER(uint32, TranslucencyGIGridPixelSizeShift)
	SHADER_PARAMETER(FIntVector, TranslucencyGIGridSize)
	SHADER_PARAMETER(uint32, UseJitter)
	SHADER_PARAMETER(FVector3f, FrameJitterOffset)
	SHADER_PARAMETER(FMatrix44f, UnjitteredClipToTranslatedWorld)
	SHADER_PARAMETER(uint32, TranslucencyVolumeTracingOctahedronResolution)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FurthestHZBTexture)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
	SHADER_PARAMETER(float, HZBMipLevel)
	SHADER_PARAMETER(float, GridCenterOffsetFromDepthBuffer)
	SHADER_PARAMETER(float, GridCenterOffsetThresholdToAcceptDepthBufferOffset)
	SHADER_PARAMETER(FVector2f, ViewportUVToHZBBufferUV)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FLumenTranslucencyLightingVolumeTraceSetupParameters, )
	SHADER_PARAMETER(float, StepFactor)
	SHADER_PARAMETER(float, MaxTraceDistance)
	SHADER_PARAMETER(float, VoxelTraceStartDistanceScale)
	SHADER_PARAMETER(float, MaxRayIntensity)
END_SHADER_PARAMETER_STRUCT()

namespace Lumen
{
	extern bool UseHardwareRayTracedTranslucencyVolume(const FSceneViewFamily& ViewFamily);
	extern bool UseLumenTranslucencyRadianceCacheReflections(const FViewInfo& View);
}

extern void HardwareRayTraceTranslucencyVolume(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FLumenCardTracingParameters& TracingParameters,
	LumenRadianceCache::FRadianceCacheInterpolationParameters RadianceCacheParameters,
	FLumenTranslucencyLightingVolumeParameters VolumeParameters,
	FLumenTranslucencyLightingVolumeTraceSetupParameters TraceSetupParameters,
	FRDGTextureRef VolumeTraceRadiance,
	FRDGTextureRef VolumeTraceHitDistance,
	ERDGPassFlags ComputePassFlags
);

namespace LumenTranslucencyVolumeRadianceCache
{
	extern LumenRadianceCache::FRadianceCacheInputs SetupRadianceCacheInputs(const FViewInfo& View);
};
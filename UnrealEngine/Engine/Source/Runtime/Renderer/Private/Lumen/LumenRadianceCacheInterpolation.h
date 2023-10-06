// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "RenderGraphResources.h"

class FViewInfo;
class FRadianceCacheState;

namespace LumenRadianceCache
{
	// Must match RadianceCacheCommon.ush
	static constexpr int32 MaxClipmaps = 6;

	static constexpr int32 MinRadianceProbeResolution = 8;

	BEGIN_SHADER_PARAMETER_STRUCT(FRadianceCacheInputs, )
		SHADER_PARAMETER(float, ReprojectionRadiusScale)
		SHADER_PARAMETER(float, ClipmapWorldExtent)
		SHADER_PARAMETER(float, ClipmapDistributionBase)
		SHADER_PARAMETER(float, InvClipmapFadeSize)
		SHADER_PARAMETER(FIntPoint, ProbeAtlasResolutionInProbes)
		SHADER_PARAMETER(uint32, RadianceProbeClipmapResolution)
		SHADER_PARAMETER(uint32, NumRadianceProbeClipmaps)
		SHADER_PARAMETER(uint32, RadianceProbeResolution)
		SHADER_PARAMETER(uint32, FinalProbeResolution)
		SHADER_PARAMETER(uint32, FinalRadianceAtlasMaxMip)
		SHADER_PARAMETER(uint32, CalculateIrradiance)
		SHADER_PARAMETER(uint32, IrradianceProbeResolution)
		SHADER_PARAMETER(uint32, OcclusionProbeResolution)
		SHADER_PARAMETER(uint32, NumProbesToTraceBudget)
		SHADER_PARAMETER(uint32, RadianceCacheStats)
	END_SHADER_PARAMETER_STRUCT()

	FRadianceCacheInputs GetDefaultRadianceCacheInputs();

	BEGIN_SHADER_PARAMETER_STRUCT(FRadianceCacheInterpolationParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FRadianceCacheInputs, RadianceCacheInputs)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, RadianceProbeIndirectionTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, RadianceCacheFinalRadianceAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, RadianceCacheFinalIrradianceAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float2>, RadianceCacheProbeOcclusionAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, RadianceCacheDepthAtlas)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, ProbeWorldOffset)
		SHADER_PARAMETER_ARRAY(FVector4f, RadianceProbeSettings, [MaxClipmaps])
		SHADER_PARAMETER_ARRAY(FVector4f, PaddedWorldPositionToRadianceProbeCoordBias, [MaxClipmaps])
		SHADER_PARAMETER_ARRAY(FVector4f, PaddedRadianceProbeCoordToWorldPositionBias, [MaxClipmaps])
		SHADER_PARAMETER(FVector2f, InvProbeFinalRadianceAtlasResolution)
		SHADER_PARAMETER(FVector2f, InvProbeFinalIrradianceAtlasResolution)
		SHADER_PARAMETER(FVector2f, InvProbeDepthAtlasResolution)
		SHADER_PARAMETER(uint32, OverrideCacheOcclusionLighting)
		SHADER_PARAMETER(uint32, ShowBlackRadianceCacheLighting)
		SHADER_PARAMETER(uint32, ProbeAtlasResolutionModuloMask)
		SHADER_PARAMETER(uint32, ProbeAtlasResolutionDivideShift)
	END_SHADER_PARAMETER_STRUCT()

	// Packed in vector to satisfy 16 byte array element alignment :
	// X=RadianceProbeClipmapTMin, Y=WorldPositionToRadianceProbeCoordScale, Z=RadianceProbeCoordToWorldPositionScale, W=[available]
	// Must match with LumenRadianceCacheInterpolation.ush
	inline void SetRadianceProbeClipmapTMin(FRadianceCacheInterpolationParameters& RadianceCacheInterpolationParameters, uint32 Index, float Value)
	{
		RadianceCacheInterpolationParameters.RadianceProbeSettings[Index].X = Value;
	}
	inline void SetWorldPositionToRadianceProbeCoordScale(FRadianceCacheInterpolationParameters& RadianceCacheInterpolationParameters, uint32 Index, float Value)
	{
		RadianceCacheInterpolationParameters.RadianceProbeSettings[Index].Y = Value;
	}
	inline void SetRadianceProbeCoordToWorldPositionScale(FRadianceCacheInterpolationParameters& RadianceCacheInterpolationParameters, uint32 Index, float Value)
	{
		RadianceCacheInterpolationParameters.RadianceProbeSettings[Index].Z = Value;
	}

	// Padded in vector to satisfy 16 byte array element alignment :
	// WorldPositionToRadianceProbeCoordBias, RadianceProbeCoordToWorldPositionBias
	// Must match with LumenRadianceCacheInterpolation.ush
	inline void SetWorldPositionToRadianceProbeCoordBias(FRadianceCacheInterpolationParameters& RadianceCacheInterpolationParameters, uint32 Index, const FVector3f& Value)
	{
		RadianceCacheInterpolationParameters.PaddedWorldPositionToRadianceProbeCoordBias[Index] = FVector4f(Value);
	}
	inline void SetRadianceProbeCoordToWorldPositionBias(FRadianceCacheInterpolationParameters& RadianceCacheInterpolationParameters, uint32 Index, const FVector3f& Value)
	{
		RadianceCacheInterpolationParameters.PaddedRadianceProbeCoordToWorldPositionBias[Index] = FVector4f(Value);
	}

	void GetInterpolationParameters(
		const FViewInfo& View, 
		FRDGBuilder& GraphBuilder, 
		const FRadianceCacheState& RadianceCacheState,
		const LumenRadianceCache::FRadianceCacheInputs& RadianceCacheInputs,
		FRadianceCacheInterpolationParameters& OutParameters);
};

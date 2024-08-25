// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "BlueNoise.h"
#include "LumenTracingUtils.h"
#include "ShaderParameterMacros.h"

BEGIN_SHADER_PARAMETER_STRUCT(FReservoirTextures, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ReservoirRayDirection)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ReservoirTraceRadiance)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ReservoirTraceHitDistance)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ReservoirTraceHitNormal)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ReservoirWeights)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FReservoirUAVs, )
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWReservoirRayDirection)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWReservoirTraceRadiance)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWReservoirTraceHitDistance)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float3>, RWReservoirTraceHitNormal)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWReservoirWeights)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FReSTIRParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DownsampledSceneDepth)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DownsampledWorldNormal)
	SHADER_PARAMETER(uint32, ReservoirDownsampleFactor)
	SHADER_PARAMETER(FIntPoint, ReservoirViewSize)
	SHADER_PARAMETER(FIntPoint, ReservoirBufferSize)
	SHADER_PARAMETER(int32, FixedJitterIndex)
	SHADER_PARAMETER(float, ResamplingNormalDotThreshold)
	SHADER_PARAMETER(float, ResamplingDepthErrorThreshold)
	SHADER_PARAMETER_STRUCT_INCLUDE(FReservoirTextures, Textures)
	SHADER_PARAMETER_STRUCT_INCLUDE(FReservoirUAVs, UAVs)
	SHADER_PARAMETER_STRUCT_REF(FBlueNoise, BlueNoise)
END_SHADER_PARAMETER_STRUCT()

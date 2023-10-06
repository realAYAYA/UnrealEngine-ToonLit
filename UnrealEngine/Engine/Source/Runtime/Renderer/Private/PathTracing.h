// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ShaderParameterMacros.h"

// this struct holds skylight parameters
BEGIN_SHADER_PARAMETER_STRUCT(FPathTracingSkylight, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SkylightTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SkylightPdf)
	SHADER_PARAMETER_SAMPLER(SamplerState, SkylightTextureSampler)
	SHADER_PARAMETER(float, SkylightInvResolution)
	SHADER_PARAMETER(int32, SkylightMipCount)
END_SHADER_PARAMETER_STRUCT()

class FRDGBuilder;
class FScene;
class FViewInfo;
class FSceneViewFamily;
class FRHIRayTracingShader;
class FGlobalShaderMap;

bool PrepareSkyTexture(FRDGBuilder& GraphBuilder, FScene* Scene, const FViewInfo& View, bool SkylightEnabled, bool UseMISCompensation, FPathTracingSkylight* SkylightParameters);
RENDERER_API FRHIRayTracingShader* GetPathTracingDefaultMissShader(const FGlobalShaderMap* ShaderMap);
RENDERER_API FRHIRayTracingShader* GetPathTracingDefaultOpaqueHitShader(const FGlobalShaderMap* ShaderMap);
RENDERER_API FRHIRayTracingShader* GetPathTracingDefaultHiddenHitShader(const FGlobalShaderMap* ShaderMap);

RENDERER_API FRHIRayTracingShader* GetGPULightmassDefaultMissShader(const FGlobalShaderMap* ShaderMap);
RENDERER_API FRHIRayTracingShader* GetGPULightmassDefaultOpaqueHitShader(const FGlobalShaderMap* ShaderMap);
RENDERER_API FRHIRayTracingShader* GetGPULightmassDefaultHiddenHitShader(const FGlobalShaderMap* ShaderMap);

class FRDGTexture;

struct FPathTracingResources
{
	FRDGTexture* DenoisedRadiance = nullptr;
	FRDGTexture* Radiance = nullptr;
	FRDGTexture* Albedo = nullptr;
	FRDGTexture* Normal = nullptr;
	FRDGTexture* Variance = nullptr;
	
	bool bPostProcessEnabled = false;
};

namespace PathTracing
{
	bool UsesDecals(const FSceneViewFamily& ViewFamily);
}

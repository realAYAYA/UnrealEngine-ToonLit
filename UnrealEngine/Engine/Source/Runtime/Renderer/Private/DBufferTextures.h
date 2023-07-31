// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraphDefinitions.h"
#include "RHIDefinitions.h"
#include "ShaderParameterMacros.h"

struct FSceneTextures;
class FSceneViewFamily;
class FSceneTextureUniformParameters;
class FViewInfo;

enum class EDecalDBufferMaskTechnique
{
	Disabled,	// DBufferMask is not enabled.
	PerPixel,	// DBufferMask is written explicitly by the shader during the DBuffer pass.
	WriteMask,	// DBufferMask is constructed after the DBuffer pass by compositing DBuffer write mask planes together in a compute shader.
};

EDecalDBufferMaskTechnique GetDBufferMaskTechnique(EShaderPlatform ShaderPlatform);

struct FDBufferTexturesDesc
{
	FRDGTextureDesc DBufferADesc;
	FRDGTextureDesc DBufferBDesc;
	FRDGTextureDesc DBufferCDesc;
	FRDGTextureDesc DBufferMaskDesc;
};

struct FDBufferTextures
{
	bool IsValid() const;

	FRDGTextureRef DBufferA = nullptr;
	FRDGTextureRef DBufferB = nullptr;
	FRDGTextureRef DBufferC = nullptr;
	FRDGTextureRef DBufferMask = nullptr;
};

FDBufferTexturesDesc GetDBufferTexturesDesc(FIntPoint Extent, EShaderPlatform ShaderPlatform);
FDBufferTextures CreateDBufferTextures(FRDGBuilder& GraphBuilder, FIntPoint Extent, EShaderPlatform ShaderPlatform);

BEGIN_SHADER_PARAMETER_STRUCT(FDBufferParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DBufferATexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DBufferBTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DBufferCTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, DBufferRenderMask)
	SHADER_PARAMETER_SAMPLER(SamplerState, DBufferATextureSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, DBufferBTextureSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, DBufferCTextureSampler)
END_SHADER_PARAMETER_STRUCT()

FDBufferParameters GetDBufferParameters(FRDGBuilder& GraphBuilder, const FDBufferTextures& DBufferTextures, EShaderPlatform ShaderPlatform);

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "HAL/Platform.h"
#include "RHICommandList.h"
#include "RenderResource.h"
#include "Serialization/MemoryLayout.h"
#include "Shader.h"
#include "ShaderParameterUtils.h"
#include "ShaderParameters.h"

class FPointerTableBase;
class FRHISamplerState;
class FRHITexture;
class FTexture;

class FOculusVertexShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOculusVertexShader, Global, RENDERCORE_API);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return true; }

	FOculusVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{}
	FOculusVertexShader() {}
};

class FOculusWhiteShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOculusWhiteShader, Global, RENDERCORE_API);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return true; }

	FOculusWhiteShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
	}
	
	FOculusWhiteShader() {}
};

class FOculusBlackShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOculusBlackShader, Global, RENDERCORE_API);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return true; }

	FOculusBlackShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
	}

	FOculusBlackShader() {}
};

class FOculusAlphaInverseShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOculusAlphaInverseShader, Global, RENDERCORE_API);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return true; }

	FOculusAlphaInverseShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		InTexture.Bind(Initializer.ParameterMap, TEXT("InTexture"), SPF_Mandatory);
		InTextureSampler.Bind(Initializer.ParameterMap, TEXT("InTextureSampler"));
	}
	FOculusAlphaInverseShader() {}

	RENDERCORE_API void SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture)
	{
		SetTextureParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), InTexture, InTextureSampler, Texture);
	}

	RENDERCORE_API void SetParameters(FRHICommandList& RHICmdList, FRHISamplerState* SamplerStateRHI, FRHITexture* TextureRHI)
	{
		SetTextureParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), InTexture, InTextureSampler, SamplerStateRHI, TextureRHI);
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, InTexture);
	LAYOUT_FIELD(FShaderResourceParameter, InTextureSampler);
};


/**
* A pixel shader for rendering a textured screen element.
*/
class FOculusCubemapPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOculusCubemapPS, Global, RENDERCORE_API);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return true; }

	FOculusCubemapPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		InTexture.Bind(Initializer.ParameterMap, TEXT("InTextureCube"), SPF_Mandatory);
		InTextureSampler.Bind(Initializer.ParameterMap, TEXT("InTextureSampler"));
		InFaceIndexParameter.Bind(Initializer.ParameterMap, TEXT("CubeFaceIndex"));
	}
	FOculusCubemapPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture, int FaceIndex)
	{
		SetTextureParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), InTexture, InTextureSampler, Texture);
		SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(), InFaceIndexParameter, FaceIndex);
	}

	void SetParameters(FRHICommandList& RHICmdList, FRHISamplerState* SamplerStateRHI, FRHITexture* TextureRHI, int FaceIndex)
	{
		SetTextureParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), InTexture, InTextureSampler, SamplerStateRHI, TextureRHI);
		SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(), InFaceIndexParameter, FaceIndex);
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, InTexture);
	LAYOUT_FIELD(FShaderResourceParameter, InTextureSampler);
	LAYOUT_FIELD(FShaderParameter, InFaceIndexParameter);
};
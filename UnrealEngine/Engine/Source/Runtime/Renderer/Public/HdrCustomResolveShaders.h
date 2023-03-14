// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "HAL/Platform.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RHIDefinitions.h"
#include "Serialization/MemoryLayout.h"
#include "Shader.h"
#include "ShaderCore.h"
#include "ShaderParameterUtils.h"
#include "ShaderParameters.h"

class FPointerTableBase;

class FHdrCustomResolveVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHdrCustomResolveVS,Global);
public:
	FHdrCustomResolveVS() {}
	FHdrCustomResolveVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader( Initializer )
	{
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}
};

class FHdrCustomResolve2xPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHdrCustomResolve2xPS,Global);
public:
	FHdrCustomResolve2xPS() {}
	FHdrCustomResolve2xPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader( Initializer )
	{
		Tex.Bind(Initializer.ParameterMap, TEXT("Tex"), SPF_Mandatory);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}

	void SetParameters(FRHICommandList& RHICmdList, FRHITexture* Texture2DMS)
	{
		FRHIPixelShader* PixelShaderRHI = RHICmdList.GetBoundPixelShader();
		SetTextureParameter(RHICmdList, PixelShaderRHI, Tex, Texture2DMS);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("HDR_CUSTOM_RESOLVE_2X"), 1);
	}

protected:
	LAYOUT_FIELD(FShaderResourceParameter, Tex);
};

class FHdrCustomResolve4xPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHdrCustomResolve4xPS,Global);
public:
	FHdrCustomResolve4xPS() {}
	FHdrCustomResolve4xPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader( Initializer )
	{
		Tex.Bind(Initializer.ParameterMap, TEXT("Tex"), SPF_Mandatory);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}

	void SetParameters(FRHICommandList& RHICmdList, FRHITexture* Texture2DMS)
	{
		FRHIPixelShader* PixelShaderRHI = RHICmdList.GetBoundPixelShader();
		SetTextureParameter(RHICmdList, PixelShaderRHI, Tex, Texture2DMS);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("HDR_CUSTOM_RESOLVE_4X"), 1);
	}

protected:
	LAYOUT_FIELD(FShaderResourceParameter, Tex);
};


class FHdrCustomResolve8xPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHdrCustomResolve8xPS,Global);
public:
	FHdrCustomResolve8xPS() {}
	FHdrCustomResolve8xPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader( Initializer )
	{
		Tex.Bind(Initializer.ParameterMap, TEXT("Tex"), SPF_Mandatory);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}

	void SetParameters(FRHICommandList& RHICmdList, FRHITexture* Texture2DMS)
	{
		FRHIPixelShader* PixelShaderRHI = RHICmdList.GetBoundPixelShader();
		SetTextureParameter(RHICmdList, PixelShaderRHI, Tex, Texture2DMS);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("HDR_CUSTOM_RESOLVE_8X"), 1);
	}

protected:
	LAYOUT_FIELD(FShaderResourceParameter, Tex);
};

class FHdrCustomResolveFMask2xPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHdrCustomResolveFMask2xPS, Global);
public:
	FHdrCustomResolveFMask2xPS() {}
	FHdrCustomResolveFMask2xPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		Tex.Bind(Initializer.ParameterMap, TEXT("Tex"), SPF_Mandatory);
		FMaskTex.Bind(Initializer.ParameterMap, TEXT("FMaskTex"), SPF_Optional);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}	

	void SetParameters(FRHICommandList& RHICmdList, FRHITexture* Texture2DMS, FRHIShaderResourceView* FMaskSRV)
	{
		FRHIPixelShader* PixelShaderRHI = RHICmdList.GetBoundPixelShader();
		SetTextureParameter(RHICmdList, PixelShaderRHI, Tex, Texture2DMS);
		SetSRVParameter(RHICmdList, PixelShaderRHI, FMaskTex, FMaskSRV);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("HDR_RESOLVE_NUM_SAMPLES"), 2);
		OutEnvironment.SetDefine(TEXT("HDR_CUSTOM_RESOLVE_USES_FMASK"), 1);
	}

protected:
	LAYOUT_FIELD(FShaderResourceParameter, Tex);
	LAYOUT_FIELD(FShaderResourceParameter, FMaskTex);
};

class FHdrCustomResolveFMask4xPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHdrCustomResolveFMask4xPS, Global);
public:
	FHdrCustomResolveFMask4xPS() {}
	FHdrCustomResolveFMask4xPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		Tex.Bind(Initializer.ParameterMap, TEXT("Tex"), SPF_Mandatory);
		FMaskTex.Bind(Initializer.ParameterMap, TEXT("FMaskTex"), SPF_Optional);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}

	void SetParameters(FRHICommandList& RHICmdList, FRHITexture* Texture2DMS, FRHIShaderResourceView* FMaskSRV)
	{
		FRHIPixelShader* PixelShaderRHI = RHICmdList.GetBoundPixelShader();
		SetTextureParameter(RHICmdList, PixelShaderRHI, Tex, Texture2DMS);
		SetSRVParameter(RHICmdList, PixelShaderRHI, FMaskTex, FMaskSRV);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("HDR_RESOLVE_NUM_SAMPLES"), 4);
		OutEnvironment.SetDefine(TEXT("HDR_CUSTOM_RESOLVE_USES_FMASK"), 1);
	}

protected:
	LAYOUT_FIELD(FShaderResourceParameter, Tex);
	LAYOUT_FIELD(FShaderResourceParameter, FMaskTex);
};


class FHdrCustomResolveFMask8xPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHdrCustomResolveFMask8xPS, Global);
public:
	FHdrCustomResolveFMask8xPS() {}
	FHdrCustomResolveFMask8xPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		Tex.Bind(Initializer.ParameterMap, TEXT("Tex"), SPF_Mandatory);
		FMaskTex.Bind(Initializer.ParameterMap, TEXT("FMaskTex"), SPF_Optional);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}

	void SetParameters(FRHICommandList& RHICmdList, FRHITexture* Texture2DMS, FRHIShaderResourceView* FMaskSRV)
	{
		FRHIPixelShader* PixelShaderRHI = RHICmdList.GetBoundPixelShader();
		SetTextureParameter(RHICmdList, PixelShaderRHI, Tex, Texture2DMS);
		SetSRVParameter(RHICmdList, PixelShaderRHI, FMaskTex, FMaskSRV);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("HDR_RESOLVE_NUM_SAMPLES"), 8);
		OutEnvironment.SetDefine(TEXT("HDR_CUSTOM_RESOLVE_USES_FMASK"), 1);
	}

protected:
	LAYOUT_FIELD(FShaderResourceParameter, Tex);
	LAYOUT_FIELD(FShaderResourceParameter, FMaskTex);
};

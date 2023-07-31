// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalShader.h"
#include "RenderUtils.h"
#include "RHIStaticStates.h"
#include "PipelineStateCache.h"
#include "ShaderParameterUtils.h"
#include "MediaShaders.h"

/**
 * Shaders which allow the conversion of NV12 texture data into RGB textures.
 */
class FWmfMediaHardwareVideoDecodingShader : public FGlobalShader
{
	DECLARE_TYPE_LAYOUT(FWmfMediaHardwareVideoDecodingShader, NonVirtual);
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) 
			|| IsPCPlatform(Parameters.Platform); // to support mobile emulation on PC
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	FWmfMediaHardwareVideoDecodingShader() {}

	FWmfMediaHardwareVideoDecodingShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TextureY.Bind(Initializer.ParameterMap, TEXT("TextureY"));
		TextureUV.Bind(Initializer.ParameterMap, TEXT("TextureUV"));

		PointClampedSamplerY.Bind(Initializer.ParameterMap, TEXT("PointClampedSamplerY"));
		BilinearClampedSamplerUV.Bind(Initializer.ParameterMap, TEXT("BilinearClampedSamplerUV"));
		BilinearClampedSamplerUVAlpha.Bind(Initializer.ParameterMap, TEXT("BilinearClampedSamplerUVAlpha"));

		ColorTransform.Bind(Initializer.ParameterMap, TEXT("ColorTransform"));
		SrgbToLinear.Bind(Initializer.ParameterMap, TEXT("SrgbToLinear"));
	}

	template<typename TShaderRHIParamRef>
	void SetParameters(
		FRHICommandListImmediate& RHICmdList,
		const TShaderRHIParamRef ShaderRHI,
		const FShaderResourceViewRHIRef& InTextureY,
		const FShaderResourceViewRHIRef& InTextureUV,
		const bool InIsOutputSrgb,
		const bool InFilterUV = true
	)
	{
		SetSRVParameter(RHICmdList, ShaderRHI, TextureY, InTextureY);
		SetSRVParameter(RHICmdList, ShaderRHI, TextureUV, InTextureUV);

		SetSamplerParameter(RHICmdList, ShaderRHI, PointClampedSamplerY, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		SetSamplerParameter(RHICmdList, ShaderRHI, BilinearClampedSamplerUV, InFilterUV ? TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI() : TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		SetSamplerParameter(RHICmdList, ShaderRHI, BilinearClampedSamplerUVAlpha, InFilterUV ? TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI() : TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		SetShaderValue(RHICmdList, ShaderRHI, ColorTransform, (FMatrix44f)MediaShaders::CombineColorTransformAndOffset(MediaShaders::YuvToRgbRec709Scaled, MediaShaders::YUVOffset8bits));
		SetShaderValue(RHICmdList, ShaderRHI, SrgbToLinear, InIsOutputSrgb ? 1 : 0); // Explicitly specify integer value, as using boolean falls over on XboxOne.
	}

	template<typename TShaderRHIParamRef>
	void SetParameters(
		FRHICommandListImmediate& RHICmdList,
		const TShaderRHIParamRef ShaderRHI,
		const FShaderResourceViewRHIRef& InTextureRGBA,
		const bool InIsOutputSrgb
	)
	{
		SetSRVParameter(RHICmdList, ShaderRHI, TextureY, InTextureRGBA);

		SetSamplerParameter(RHICmdList, ShaderRHI, PointClampedSamplerY, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		SetSamplerParameter(RHICmdList, ShaderRHI, BilinearClampedSamplerUV, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		SetShaderValue(RHICmdList, ShaderRHI, ColorTransform, (FMatrix44f)MediaShaders::CombineColorTransformAndOffset(MediaShaders::YuvToRgbRec709Scaled, MediaShaders::YUVOffset8bits));
		SetShaderValue(RHICmdList, ShaderRHI, SrgbToLinear, InIsOutputSrgb ? 1 : 0); // Explicitly specify integer value, as using boolean falls over on XboxOne.
	}

private:
	
	LAYOUT_FIELD(FShaderResourceParameter, TextureY);
	LAYOUT_FIELD(FShaderResourceParameter, TextureUV);
	LAYOUT_FIELD(FShaderResourceParameter, PointClampedSamplerY);
	LAYOUT_FIELD(FShaderResourceParameter, BilinearClampedSamplerUV);
	LAYOUT_FIELD(FShaderResourceParameter, BilinearClampedSamplerUVAlpha);
	LAYOUT_FIELD(FShaderParameter, ColorTransform);
	LAYOUT_FIELD(FShaderParameter, SrgbToLinear);
};

class FHardwareVideoDecodingVS : public FWmfMediaHardwareVideoDecodingShader
{
	DECLARE_SHADER_TYPE(FHardwareVideoDecodingVS, Global);

public:

	/** Default constructor. */
	FHardwareVideoDecodingVS() {}

	/** Initialization constructor. */
	FHardwareVideoDecodingVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FWmfMediaHardwareVideoDecodingShader(Initializer)
	{
	}
};

class FHardwareVideoDecodingPS : public FWmfMediaHardwareVideoDecodingShader
{
	DECLARE_SHADER_TYPE(FHardwareVideoDecodingPS, Global);

public:

	/** Default constructor. */
	FHardwareVideoDecodingPS() {}

	/** Initialization constructor. */
	FHardwareVideoDecodingPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FWmfMediaHardwareVideoDecodingShader(Initializer)
	{ }
};

class FHardwareVideoDecodingPassThroughPS : public FWmfMediaHardwareVideoDecodingShader
{
	DECLARE_SHADER_TYPE(FHardwareVideoDecodingPassThroughPS, Global);

public:

	/** Default constructor. */
	FHardwareVideoDecodingPassThroughPS() {}

	/** Initialization constructor. */
	FHardwareVideoDecodingPassThroughPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FWmfMediaHardwareVideoDecodingShader(Initializer)
	{ }
};

class FHardwareVideoDecodingY416PS : public FWmfMediaHardwareVideoDecodingShader
{
	DECLARE_SHADER_TYPE(FHardwareVideoDecodingY416PS, Global);

public:

	/** Default constructor. */
	FHardwareVideoDecodingY416PS() {}

	/** Initialization constructor. */
	FHardwareVideoDecodingY416PS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FWmfMediaHardwareVideoDecodingShader(Initializer)
	{ }
};


class FHardwareVideoDecodingYCoCgPS : public FWmfMediaHardwareVideoDecodingShader
{
	DECLARE_SHADER_TYPE(FHardwareVideoDecodingYCoCgPS, Global);

public:

	/** Default constructor. */
	FHardwareVideoDecodingYCoCgPS() {}

	/** Initialization constructor. */
	FHardwareVideoDecodingYCoCgPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FWmfMediaHardwareVideoDecodingShader(Initializer)
	{ }
};


class FHardwareVideoDecodingYCoCgAlphaPS : public FWmfMediaHardwareVideoDecodingShader
{
	DECLARE_SHADER_TYPE(FHardwareVideoDecodingYCoCgAlphaPS, Global);

public:

	/** Default constructor. */
	FHardwareVideoDecodingYCoCgAlphaPS() {}

	/** Initialization constructor. */
	FHardwareVideoDecodingYCoCgAlphaPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FWmfMediaHardwareVideoDecodingShader(Initializer)
	{ }
};



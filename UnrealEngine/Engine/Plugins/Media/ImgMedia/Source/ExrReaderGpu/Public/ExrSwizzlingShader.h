// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if defined(PLATFORM_WINDOWS) && PLATFORM_WINDOWS

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "ShaderParameterStruct.h"

// The vertex shader used by DrawScreenPass to draw a rectangle.
class EXRREADERGPU_API FExrSwizzleVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FExrSwizzleVS);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters&)
	{
		return true;
	}

	FExrSwizzleVS() = default;
	FExrSwizzleVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

/** Pixel shader swizzle RGB planar buffer data into proper RGBA texture. */
class EXRREADERGPU_API FExrSwizzlePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FExrSwizzlePS);
	SHADER_USE_PARAMETER_STRUCT(FExrSwizzlePS, FGlobalShader);

	/** If the provided buffer is RGBA the shader would work slightly differently to RGB. */
	class FRgbaSwizzle : SHADER_PERMUTATION_INT("NUM_CHANNELS", 4);
	class FRenderTiles : SHADER_PERMUTATION_BOOL("RENDER_TILES");
	class FCustomExr : SHADER_PERMUTATION_BOOL("CUSTOM_EXR");
	class FPartialTiles : SHADER_PERMUTATION_BOOL("PARTIAL_TILES");
	using FPermutationDomain = TShaderPermutationDomain<FRgbaSwizzle, FRenderTiles, FCustomExr, FPartialTiles>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		return true;
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<uint>, UnswizzledBuffer)
		SHADER_PARAMETER_SRV(StructuredBuffer<FTileDesc>, TileDescBuffer)
		SHADER_PARAMETER(FIntPoint, TextureSize)
		SHADER_PARAMETER(FIntPoint, TileSize)
		SHADER_PARAMETER(FIntPoint, NumTiles)
		SHADER_PARAMETER(int32, NumChannels)
	END_SHADER_PARAMETER_STRUCT()
};
#endif
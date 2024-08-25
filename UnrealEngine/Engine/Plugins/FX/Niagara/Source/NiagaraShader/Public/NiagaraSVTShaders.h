// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

class FNiagaraCopySVTToDenseBufferCS : public FGlobalShader
{
	DECLARE_EXPORTED_GLOBAL_SHADER(FNiagaraCopySVTToDenseBufferCS, NIAGARASHADER_API);
	SHADER_USE_PARAMETER_STRUCT(FNiagaraCopySVTToDenseBufferCS, FGlobalShader);
	
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, NIAGARASHADER_API)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, DestinationBuffer)
		SHADER_PARAMETER_SAMPLER(SamplerState, TileDataTextureSampler)
		SHADER_PARAMETER_TEXTURE(Texture3D<uint>, SparseVolumeTexturePageTable)
		SHADER_PARAMETER_TEXTURE(Texture3D, SparseVolumeTextureA)	
		SHADER_PARAMETER(FUintVector4, PackedSVTUniforms0)
		SHADER_PARAMETER(FUintVector4, PackedSVTUniforms1)
		SHADER_PARAMETER(FIntVector, TextureSize)
		SHADER_PARAMETER(int32, MipLevel)
	END_SHADER_PARAMETER_STRUCT()
};

// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenColorIOShader.h: Shader base classes
=============================================================================*/

#pragma once

#include "Containers/ContainersFwd.h"
#include "GlobalShader.h"
#include "OpenColorIOShaderType.h"
#include "OpenColorIOShared.h"
#include "ShaderParameters.h"
#include "ShaderParameterStruct.h"

class FTextureResource;
class UClass;

BEGIN_SHADER_PARAMETER_STRUCT(FOpenColorIOPixelShaderParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, InputTextureSampler)
	SHADER_PARAMETER(float, Gamma)
	
	SHADER_PARAMETER_TEXTURE(Texture3D, Ocio_lut3d_0)
	SHADER_PARAMETER_SAMPLER(SamplerState, Ocio_lut3d_0Sampler)
	SHADER_PARAMETER_TEXTURE(Texture3D, Ocio_lut3d_1)
	SHADER_PARAMETER_SAMPLER(SamplerState, Ocio_lut3d_1Sampler)
	SHADER_PARAMETER_TEXTURE(Texture3D, Ocio_lut3d_2)
	SHADER_PARAMETER_SAMPLER(SamplerState, Ocio_lut3d_2Sampler)
	SHADER_PARAMETER_TEXTURE(Texture3D, Ocio_lut3d_3)
	SHADER_PARAMETER_SAMPLER(SamplerState, Ocio_lut3d_3Sampler)
	SHADER_PARAMETER_TEXTURE(Texture3D, Ocio_lut3d_4)
	SHADER_PARAMETER_SAMPLER(SamplerState, Ocio_lut3d_4Sampler)
	SHADER_PARAMETER_TEXTURE(Texture3D, Ocio_lut3d_5)
	SHADER_PARAMETER_SAMPLER(SamplerState, Ocio_lut3d_5Sampler)
	SHADER_PARAMETER_TEXTURE(Texture3D, Ocio_lut3d_6)
	SHADER_PARAMETER_SAMPLER(SamplerState, Ocio_lut3d_6Sampler)
	SHADER_PARAMETER_TEXTURE(Texture3D, Ocio_lut3d_7)
	SHADER_PARAMETER_SAMPLER(SamplerState, Ocio_lut3d_7Sampler)

	SHADER_PARAMETER_TEXTURE(Texture2D, Ocio_lut1d_0)
	SHADER_PARAMETER_SAMPLER(SamplerState, Ocio_lut1d_0Sampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, Ocio_lut1d_1)
	SHADER_PARAMETER_SAMPLER(SamplerState, Ocio_lut1d_1Sampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, Ocio_lut1d_2)
	SHADER_PARAMETER_SAMPLER(SamplerState, Ocio_lut1d_2Sampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, Ocio_lut1d_3)
	SHADER_PARAMETER_SAMPLER(SamplerState, Ocio_lut1d_3Sampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, Ocio_lut1d_4)
	SHADER_PARAMETER_SAMPLER(SamplerState, Ocio_lut1d_4Sampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, Ocio_lut1d_5)
	SHADER_PARAMETER_SAMPLER(SamplerState, Ocio_lut1d_5Sampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, Ocio_lut1d_6)
	SHADER_PARAMETER_SAMPLER(SamplerState, Ocio_lut1d_6Sampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, Ocio_lut1d_7)
	SHADER_PARAMETER_SAMPLER(SamplerState, Ocio_lut1d_7Sampler)
	
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

namespace OpenColorIOShader
{
	static constexpr TCHAR OpenColorIOShaderFunctionName[] = TEXT("OCIOConvert");
	static constexpr uint32 Lut3dEdgeLength = 65;

	// Combined maximum of either Ocio_lut1d or Ocio_lut3d, since the generated shaders will never use the same slots for both types.
	static constexpr uint32 MaximumTextureSlots = 8;
}

class OPENCOLORIO_API FOpenColorIOPixelShader : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FOpenColorIOPixelShader, OpenColorIO);
	SHADER_USE_PARAMETER_STRUCT(FOpenColorIOPixelShader, FGlobalShader);

	using FParameters = FOpenColorIOPixelShaderParameters;
	using FPermutationParameters = FOpenColorIOShaderPermutationParameters;

	static bool ShouldCompilePermutation(const FOpenColorIOShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

OPENCOLORIO_API void OpenColorIOBindTextureResources(FOpenColorIOPixelShaderParameters* Parameters, const TSortedMap<int32, FTextureResource*>& InTextureResources);

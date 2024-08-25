// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenColorIOShader.h: Shader base classes
=============================================================================*/

#pragma once

#include "Containers/ContainersFwd.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "GlobalShader.h"
#endif
#include "OpenColorIOShaderType.h"
#include "OpenColorIOShared.h"
#include "ShaderParameters.h"
#include "ShaderParameterStruct.h"

class FTextureResource;
class UClass;
class FRHITexture;

BEGIN_SHADER_PARAMETER_STRUCT(FOpenColorIOPixelShaderParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, InputTextureSampler)
	SHADER_PARAMETER(float, Gamma)
	SHADER_PARAMETER(uint32, TransformAlpha)
	
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
/** Fields moved to OpenColorIOWrapper.h
 *
 *	static constexpr TCHAR OpenColorIOShaderFunctionName[] = TEXT("OCIOConvert");
 *	static constexpr uint32 Lut3dEdgeLength = 65;
*/

	// Combined maximum of either Ocio_lut1d or Ocio_lut3d, since the generated shaders will never use the same slots for both types.
	static constexpr uint32 MaximumTextureSlots = 8;
}

class FOpenColorIOPixelShader : public FGlobalShader
{
public:
	DECLARE_EXPORTED_SHADER_TYPE(FOpenColorIOPixelShader, OpenColorIO, OPENCOLORIO_API);
	SHADER_USE_PARAMETER_STRUCT(FOpenColorIOPixelShader, FGlobalShader);

	using FParameters = FOpenColorIOPixelShaderParameters;
	using FPermutationParameters = FOpenColorIOShaderPermutationParameters;

	static bool ShouldCompilePermutation(const FOpenColorIOShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

BEGIN_SHADER_PARAMETER_STRUCT(FOpenColorIOInvalidShaderParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, InputTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, MiniFontTexture)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class OPENCOLORIO_API FOpenColorIOInvalidPixelShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FOpenColorIOInvalidPixelShader);
	SHADER_USE_PARAMETER_STRUCT(FOpenColorIOInvalidPixelShader, FGlobalShader);

	using FParameters = FOpenColorIOInvalidShaderParameters;
};

OPENCOLORIO_API bool OpenColorIOBindTextureResources(FOpenColorIOPixelShaderParameters* Parameters, const TSortedMap<int32, FTextureResource*>& InTextureResources);

FRHITexture* OpenColorIOGetMiniFontTexture();

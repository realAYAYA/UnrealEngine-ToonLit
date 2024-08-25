// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Job/Job.h"
#include "FxMat/FxMaterial.h"
#include "Model/Mix/MixUpdateCycle.h"
#include <DataDrivenShaderPlatformInfo.h>

//////////////////////////////////////////////////////////////////////////
class ULayer_Textured;
namespace
{
}
class FSH_NormalBlendWithHeightMask : public FSH_Base
{
public:
	DECLARE_GLOBAL_SHADER(FSH_NormalBlendWithHeightMask);
	SHADER_USE_PARAMETER_STRUCT(FSH_NormalBlendWithHeightMask, FSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, SourceTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D, DestinationTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D, Mask)
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER(float, Opacity)
		SHADER_PARAMETER(float, LerpRGB)
		SHADER_PARAMETER(float, LerpAlpha)
	END_SHADER_PARAMETER_STRUCT()


public:	
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& params, FShaderCompilerEnvironment& env)
	{
	}
};
template <> void SetupDefaultParameters(FSH_NormalBlendWithHeightMask::FParameters& params);

typedef FxMaterial_Normal<VSH_Simple, FSH_NormalBlendWithHeightMask>	Fx_NormalBlendWithHeightMask;

//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API T_BlendWithHeightMask 
{
public:
	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
};

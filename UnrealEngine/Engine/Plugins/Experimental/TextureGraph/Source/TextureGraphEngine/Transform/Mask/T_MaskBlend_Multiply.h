// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Job/Job.h"
#include "FxMat/FxMaterial.h"
#include "Model/Mix/MixUpdateCycle.h"
#include <DataDrivenShaderPlatformInfo.h>

//////////////////////////////////////////////////////////////////////////
/// Shader
//////////////////////////////////////////////////////////////////////////
class FSH_MaskBlend_Multiply : public FSH_Base
{
public:
	DECLARE_GLOBAL_SHADER(FSH_MaskBlend_Multiply);
	SHADER_USE_PARAMETER_STRUCT(FSH_MaskBlend_Multiply, FSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(float, MaskFactor0)
		SHADER_PARAMETER_TEXTURE(Texture2D, Mask0)

		SHADER_PARAMETER(float, MaskFactor1)
		SHADER_PARAMETER_TEXTURE(Texture2D, Mask1)
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
template <> void SetupDefaultParameters(FSH_MaskBlend_Multiply::FParameters& params);

typedef FxMaterial_Normal<VSH_Simple, FSH_MaskBlend_Multiply>	Fx_BlendMode_Multiply;

class UImageMask;

class TEXTUREGRAPHENGINE_API T_MaskBlend_Multiply
{
private:


public:
									T_MaskBlend_Multiply();
	virtual							~T_MaskBlend_Multiply();
};

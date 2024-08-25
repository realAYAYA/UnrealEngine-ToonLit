// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Job/Job.h"
#include "FxMat/FxMaterial.h"
#include "Model/Mix/MixUpdateCycle.h"
#include <DataDrivenShaderPlatformInfo.h>

//////////////////////////////////////////////////////////////////////////
/// Simple grayscale shader
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_Grayscale : public FSH_Base
{
public:
	SHADER_USE_PARAMETER_STRUCT(FSH_Grayscale, FSH_Base);
	DECLARE_GLOBAL_SHADER(FSH_Grayscale);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, SourceTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool						ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}
	static void						ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& params, FShaderCompilerEnvironment& env) {}
};

//////////////////////////////////////////////////////////////////////////
/// Simple levels shader
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_Levels : public FSH_Base
{
public:
	SHADER_USE_PARAMETER_STRUCT(FSH_Levels, FSH_Base);
	DECLARE_GLOBAL_SHADER(FSH_Levels);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, Input)
		SHADER_PARAMETER(float, MinValue)
		SHADER_PARAMETER(float, MaxValue)
		SHADER_PARAMETER(float, Gamma)
	END_SHADER_PARAMETER_STRUCT()

	static bool						ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}
	static void						ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& params, FShaderCompilerEnvironment& env) {}
};


//////////////////////////////////////////////////////////////////////////
/// Simple levels shader
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_Threshold : public FSH_Base
{
public:
	SHADER_USE_PARAMETER_STRUCT(FSH_Threshold, FSH_Base);
	DECLARE_GLOBAL_SHADER(FSH_Threshold);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, Input)
		SHADER_PARAMETER(float, Threshold)
		END_SHADER_PARAMETER_STRUCT()

		static bool						ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}
	static void						ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& params, FShaderCompilerEnvironment& env) {}
};

//////////////////////////////////////////////////////////////////////////
/// Simple HSV shader
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_HSV : public FSH_Base
{
public:
	SHADER_USE_PARAMETER_STRUCT(FSH_HSV, FSH_Base);
	DECLARE_GLOBAL_SHADER(FSH_HSV);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, Input)
		SHADER_PARAMETER(float, Hue)
		SHADER_PARAMETER(float, Saturation)
		SHADER_PARAMETER(float, Value)
	END_SHADER_PARAMETER_STRUCT()

	static bool						ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}
	static void						ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& params, FShaderCompilerEnvironment& env) {}
};

//////////////////////////////////////////////////////////////////////////
/// Simple RGB2HSV shader
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_RGB2HSV : public FSH_Base
{
public:
	SHADER_USE_PARAMETER_STRUCT(FSH_RGB2HSV, FSH_Base);
	DECLARE_GLOBAL_SHADER(FSH_RGB2HSV);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, Input)
	END_SHADER_PARAMETER_STRUCT()

	static bool						ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}
	static void						ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& params, FShaderCompilerEnvironment& env) {}
};

//////////////////////////////////////////////////////////////////////////
/// Simple HSV2RGB shader
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_HSV2RGB : public FSH_Base
{
public:
	SHADER_USE_PARAMETER_STRUCT(FSH_HSV2RGB, FSH_Base);
	DECLARE_GLOBAL_SHADER(FSH_HSV2RGB);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, Input)
	END_SHADER_PARAMETER_STRUCT()

	static bool						ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}
	static void						ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& params, FShaderCompilerEnvironment& env) {}
};

//////////////////////////////////////////////////////////////////////////
/// Helper C++ class
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API T_Color
{
public:
	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
};

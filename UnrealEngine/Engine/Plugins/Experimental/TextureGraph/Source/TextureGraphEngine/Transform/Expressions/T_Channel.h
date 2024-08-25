// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Job/Job.h"
#include "FxMat/FxMaterial.h"
#include "Model/Mix/MixUpdateCycle.h"
#include <DataDrivenShaderPlatformInfo.h>

//////////////////////////////////////////////////////////////////////////
/// Basic Channel Op
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_ChannelSplitter : public FSH_Base
{
public:
	SHADER_USE_PARAMETER_STRUCT(FSH_ChannelSplitter, FSH_Base);

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
/// Red
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_ChannelSplitter_Red : public FSH_ChannelSplitter
{
public:
	DECLARE_GLOBAL_SHADER(FSH_ChannelSplitter_Red);
	SHADER_USE_PARAMETER_STRUCT(FSH_ChannelSplitter_Red, FSH_ChannelSplitter);
};

//////////////////////////////////////////////////////////////////////////
/// Green
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_ChannelSplitter_Green : public FSH_ChannelSplitter
{
public:
	DECLARE_GLOBAL_SHADER(FSH_ChannelSplitter_Green);
	SHADER_USE_PARAMETER_STRUCT(FSH_ChannelSplitter_Green, FSH_ChannelSplitter);
};

//////////////////////////////////////////////////////////////////////////
/// Red
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_ChannelSplitter_Blue : public FSH_ChannelSplitter
{
public:
	DECLARE_GLOBAL_SHADER(FSH_ChannelSplitter_Blue);
	SHADER_USE_PARAMETER_STRUCT(FSH_ChannelSplitter_Blue, FSH_ChannelSplitter);
};

//////////////////////////////////////////////////////////////////////////
/// Red
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_ChannelSplitter_Alpha : public FSH_ChannelSplitter
{
public:
	DECLARE_GLOBAL_SHADER(FSH_ChannelSplitter_Alpha);
	SHADER_USE_PARAMETER_STRUCT(FSH_ChannelSplitter_Alpha, FSH_ChannelSplitter);
};

//////////////////////////////////////////////////////////////////////////
/// Basic Channel Op
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_ChannelCombiner : public FSH_Base
{
public:
	DECLARE_GLOBAL_SHADER(FSH_ChannelCombiner);
	SHADER_USE_PARAMETER_STRUCT(FSH_ChannelCombiner, FSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, SourceRed)
		SHADER_PARAMETER_TEXTURE(Texture2D, SourceGreen)
		SHADER_PARAMETER_TEXTURE(Texture2D, SourceBlue)
		SHADER_PARAMETER_TEXTURE(Texture2D, SourceAlpha)
	END_SHADER_PARAMETER_STRUCT()

	static bool						ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return true; }
	static void						ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& params, FShaderCompilerEnvironment& env) {}
};

//////////////////////////////////////////////////////////////////////////
/// Helper C++ class
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API T_Channel
{
public:
	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
};

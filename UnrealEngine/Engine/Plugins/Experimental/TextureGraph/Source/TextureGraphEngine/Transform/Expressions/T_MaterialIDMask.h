// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include <2D/BlendModes.h>

#include "CoreMinimal.h"
#include "T_ExtractMaterialIds.h"
#include "FxMat/FxMaterial.h"
#include "Job/Job.h"
#include "Model/Mix/MixUpdateCycle.h"
#include <DataDrivenShaderPlatformInfo.h>

//////////////////////////////////////////////////////////////////////////
/// Shader
//////////////////////////////////////////////////////////////////////////

class FSH_MaterialIDMask : public FSH_Base
{
public:

	DECLARE_GLOBAL_SHADER(FSH_MaterialIDMask);
	SHADER_USE_PARAMETER_STRUCT(FSH_MaterialIDMask, FSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, MaterialIDTexture)
		SHADER_PARAMETER_ARRAY(FVector4f, Buckets, [HSVBucket::HBuckets * HSVBucket::SBuckets * HSVBucket::VBuckets])
		SHADER_PARAMETER_ARRAY(FVector4f, ActiveColors, [128])
		SHADER_PARAMETER(int, ActiveColorsCount)
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
	END_SHADER_PARAMETER_STRUCT()

	static bool	ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParams, FShaderCompilerEnvironment& InEnv)
	{
	}

};

/**
 * 
 */
class TEXTUREGRAPHENGINE_API T_MaterialIDMask
{
public:
	T_MaterialIDMask();
	~T_MaterialIDMask();

	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////

	static TiledBlobPtr				Create(MixUpdateCyclePtr InCycle, TiledBlobPtr InMaterialIDTexture, const TArray<FLinearColor>& InActiveColors, const int32& ActiveColorsCount, BufferDescriptor DesiredDesc, int InTargetId);
};

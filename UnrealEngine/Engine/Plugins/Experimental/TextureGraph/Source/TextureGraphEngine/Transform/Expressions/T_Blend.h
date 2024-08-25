// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include <2D/BlendModes.h>

#include "CoreMinimal.h"
#include "Job/Job.h"
#include "FxMat/FxMaterial.h"
#include "Model/Mix/MixUpdateCycle.h"
#include <DataDrivenShaderPlatformInfo.h>

//////////////////////////////////////////////////////////////////////////
/// Shader
//////////////////////////////////////////////////////////////////////////

class TEXTUREGRAPHENGINE_API FSH_BlendBase : public FSH_Base
{
public:
	SHADER_USE_PARAMETER_STRUCT(FSH_BlendBase, FSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, BackgroundTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D, ForegroundTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D, MaskTexture)
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER(float, Opacity)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParams, FShaderCompilerEnvironment& InEnv)
	{
	}
};

//////////////////////////////////////////////////////////////////////////
/// Normal
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_BlendNormal : public FSH_BlendBase
{
public:
	DECLARE_GLOBAL_SHADER(FSH_BlendNormal);
	SHADER_USE_PARAMETER_STRUCT(FSH_BlendNormal, FSH_BlendBase);
};

//////////////////////////////////////////////////////////////////////////
/// Add
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_BlendAdd : public FSH_BlendBase
{
public:
	DECLARE_GLOBAL_SHADER(FSH_BlendAdd);
	SHADER_USE_PARAMETER_STRUCT(FSH_BlendAdd, FSH_BlendBase);
};

//////////////////////////////////////////////////////////////////////////
/// Add
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_BlendSubtract : public FSH_BlendBase
{
public:
	DECLARE_GLOBAL_SHADER(FSH_BlendSubtract);
	SHADER_USE_PARAMETER_STRUCT(FSH_BlendSubtract, FSH_BlendBase);
};

//////////////////////////////////////////////////////////////////////////
/// Multiply
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_BlendMultiply : public FSH_BlendBase
{
public:
	DECLARE_GLOBAL_SHADER(FSH_BlendMultiply);
	SHADER_USE_PARAMETER_STRUCT(FSH_BlendMultiply, FSH_BlendBase);
};

//////////////////////////////////////////////////////////////////////////
/// Divide
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_BlendDivide : public FSH_BlendBase
{
public:
	DECLARE_GLOBAL_SHADER(FSH_BlendDivide);
	SHADER_USE_PARAMETER_STRUCT(FSH_BlendDivide, FSH_BlendBase);
};
//////////////////////////////////////////////////////////////////////////
/// Difference
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_BlendDifference : public FSH_BlendBase
{
public:
	DECLARE_GLOBAL_SHADER(FSH_BlendDifference);
	SHADER_USE_PARAMETER_STRUCT(FSH_BlendDifference, FSH_BlendBase);
};

//////////////////////////////////////////////////////////////////////////
/// Max
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_BlendMax : public FSH_BlendBase
{
public:
	DECLARE_GLOBAL_SHADER(FSH_BlendMax);
	SHADER_USE_PARAMETER_STRUCT(FSH_BlendMax, FSH_BlendBase);
};

//////////////////////////////////////////////////////////////////////////
/// Min
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_BlendMin : public FSH_BlendBase
{
public:
	DECLARE_GLOBAL_SHADER(FSH_BlendMin);
	SHADER_USE_PARAMETER_STRUCT(FSH_BlendMin, FSH_BlendBase);
};

//////////////////////////////////////////////////////////////////////////
/// Step
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_BlendStep : public FSH_BlendBase
{
public:
	DECLARE_GLOBAL_SHADER(FSH_BlendStep);
	SHADER_USE_PARAMETER_STRUCT(FSH_BlendStep, FSH_BlendBase);
};

//////////////////////////////////////////////////////////////////////////
/// Overlay
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_BlendOverlay : public FSH_BlendBase
{
public:
	DECLARE_GLOBAL_SHADER(FSH_BlendOverlay);
	SHADER_USE_PARAMETER_STRUCT(FSH_BlendOverlay, FSH_BlendBase);
};

//////////////////////////////////////////////////////////////////////////
/// Distort
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_BlendDistort : public FSH_BlendBase
{
public:
	DECLARE_GLOBAL_SHADER(FSH_BlendDistort);
	SHADER_USE_PARAMETER_STRUCT(FSH_BlendDistort, FSH_BlendBase);
};
/**
 * 
 */
class TEXTUREGRAPHENGINE_API T_Blend
{
public:
	T_Blend();
	~T_Blend();

	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////

	static TiledBlobPtr				Create(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForeGroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId, EBlendModes InBlendMode);
	static TiledBlobPtr				CreateNormal(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForeGroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId);
	static TiledBlobPtr				CreateAdd(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForeGroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId);
	static TiledBlobPtr				CreateSubtract(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForeGroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId);
	static TiledBlobPtr				CreateMultiply(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForeGroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId);
	static TiledBlobPtr				CreateDivide(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForeGroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId);
	static TiledBlobPtr				CreateDifference(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForeGroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId);
	static TiledBlobPtr				CreateMax(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForeGroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId);
	static TiledBlobPtr				CreateMin(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForeGroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId);
	static TiledBlobPtr				CreateStep(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForeGroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId);
	static TiledBlobPtr				CreateOverlay(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForeGroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId);
	static TiledBlobPtr				CreateDistort(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr InBackgroundTexture, TiledBlobPtr InForeGroundTexture, TiledBlobPtr InMask, float InOpacity, int InTargetId);
};

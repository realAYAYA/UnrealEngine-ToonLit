// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Job/Job.h"
#include "FxMat/FxMaterial.h"
#include "Model/Mix/MixUpdateCycle.h"
#include <DataDrivenShaderPlatformInfo.h>

//////////////////////////////////////////////////////////////////////////
/// Clamp Shader
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_Clamp : public FSH_Base
{
public:
	DECLARE_GLOBAL_SHADER(FSH_Clamp);
	SHADER_USE_PARAMETER_STRUCT(FSH_Clamp, FSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, SourceTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D, MinValue)
		SHADER_PARAMETER_TEXTURE(Texture2D, MaxValue)
	END_SHADER_PARAMETER_STRUCT()

public:

	TEXTURE_ENGINE_DEFAULT_PERMUTATION;
	TEXTUREGRAPH_ENGINE_DEFAULT_COMPILATION_ENV;
};

//////////////////////////////////////////////////////////////////////////
/// Clamp Shader
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_SmoothStep : public FSH_Base
{
public:
	DECLARE_GLOBAL_SHADER(FSH_SmoothStep);
	SHADER_USE_PARAMETER_STRUCT(FSH_SmoothStep, FSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, SourceTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D, MinValue)
		SHADER_PARAMETER_TEXTURE(Texture2D, MaxValue)
	END_SHADER_PARAMETER_STRUCT()

public:

	TEXTURE_ENGINE_DEFAULT_PERMUTATION;
	TEXTUREGRAPH_ENGINE_DEFAULT_COMPILATION_ENV;
};

//////////////////////////////////////////////////////////////////////////
/// Min Shader
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_Min : public FSH_Base
{
public:
	DECLARE_GLOBAL_SHADER(FSH_Min);
	SHADER_USE_PARAMETER_STRUCT(FSH_Min, FSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, Input1)
		SHADER_PARAMETER_TEXTURE(Texture2D, Input2)
	END_SHADER_PARAMETER_STRUCT()

public:

	TEXTURE_ENGINE_DEFAULT_PERMUTATION;
	TEXTUREGRAPH_ENGINE_DEFAULT_COMPILATION_ENV;
};

//////////////////////////////////////////////////////////////////////////
/// Max Shader
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_Max : public FSH_Base
{
public:
	DECLARE_GLOBAL_SHADER(FSH_Max);
	SHADER_USE_PARAMETER_STRUCT(FSH_Max, FSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, Input1)
		SHADER_PARAMETER_TEXTURE(Texture2D, Input2)
	END_SHADER_PARAMETER_STRUCT()

public:

	TEXTURE_ENGINE_DEFAULT_PERMUTATION;
	TEXTUREGRAPH_ENGINE_DEFAULT_COMPILATION_ENV;
};

class TEXTUREGRAPHENGINE_API T_Clamp
{
public:
	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static TiledBlobPtr				CreateClamp(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, TiledBlobPtr Source, TiledBlobPtr MinValue, TiledBlobPtr MaxValue, int32 TargetId);
	static TiledBlobPtr				CreateSmoothStep(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, TiledBlobPtr Source, TiledBlobPtr MinValue, TiledBlobPtr MaxValue, int32 TargetId);
	static TiledBlobPtr				CreateMin(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, TiledBlobPtr Input1, TiledBlobPtr Input2, int32 TargetId);
	static TiledBlobPtr				CreateMax(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, TiledBlobPtr Input1, TiledBlobPtr Input2, int32 TargetId);
};

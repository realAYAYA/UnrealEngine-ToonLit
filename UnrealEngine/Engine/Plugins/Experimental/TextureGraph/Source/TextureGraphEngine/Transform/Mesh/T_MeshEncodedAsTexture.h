// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "FxMat/FxMaterial.h"
#include "Job/Job.h"
#include "Model/Mix/MixUpdateCycle.h"
#include "UObject/NoExportTypes.h"
#include <DataDrivenShaderPlatformInfo.h>

class ULayer_Textured;

////////////////////////////////////////////////////////////////////////////
class VSH_MeshTexture : public VSH_Base
{
public:
	DECLARE_GLOBAL_SHADER(VSH_MeshTexture);
	SHADER_USE_PARAMETER_STRUCT(VSH_MeshTexture, VSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}

	~VSH_MeshTexture() { FShaderType::Uninitialize(); }
};

////////////////////////////////////////////////////////////////////////////
class VSH_MeshTexture_WorldPos : public VSH_Base
{
public:
	DECLARE_GLOBAL_SHADER(VSH_MeshTexture_WorldPos);
	SHADER_USE_PARAMETER_STRUCT(VSH_MeshTexture_WorldPos, VSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector3f, BoundsMin)
		SHADER_PARAMETER(FVector3f, InvBoundsDiameter)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}

	~VSH_MeshTexture_WorldPos() { FShaderType::Uninitialize(); }
};

//////////////////////////////////////////////////////////////////////////
class FSH_MeshTexture_WorldPos : public FSH_Base
{
public:
	DECLARE_GLOBAL_SHADER(FSH_MeshTexture_WorldPos);
	SHADER_USE_PARAMETER_STRUCT(FSH_MeshTexture_WorldPos, FSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
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

//////////////////////////////////////////////////////////////////////////
class FSH_MeshTexture_WorldNormals : public FSH_Base
{
public:
	DECLARE_GLOBAL_SHADER(FSH_MeshTexture_WorldNormals);
	SHADER_USE_PARAMETER_STRUCT(FSH_MeshTexture_WorldNormals, FSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
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

//////////////////////////////////////////////////////////////////////////
class FSH_MeshTexture_WorldTangents : public FSH_Base
{
public:
	DECLARE_GLOBAL_SHADER(FSH_MeshTexture_WorldTangents);
	SHADER_USE_PARAMETER_STRUCT(FSH_MeshTexture_WorldTangents, FSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
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

//////////////////////////////////////////////////////////////////////////
class FSH_MeshTexture_WorldUVMask : public FSH_Base
{
public:
	DECLARE_GLOBAL_SHADER(FSH_MeshTexture_WorldUVMask);
	SHADER_USE_PARAMETER_STRUCT(FSH_MeshTexture_WorldUVMask, FSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
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

//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API T_MeshEncodedAsTexture 
{
public:
	static int32					s_minMeshmapRes;			// We need to create mesh maps in 4k. Otherwise we get artifacts when using in curvature and painting. 
									T_MeshEncodedAsTexture();
	virtual							~T_MeshEncodedAsTexture();

	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static TiledBlobPtr				Create_WorldPos(MixUpdateCyclePtr cycle, int32 targetId);
	static TiledBlobPtr				Create_WorldNormals(MixUpdateCyclePtr cycle, int32 targetId);
	static TiledBlobPtr				Create_WorldTangents(MixUpdateCyclePtr cycle, int32 targetId);
	static TiledBlobPtr				Create_WorldUVMask(MixUpdateCyclePtr cycle, int32 targetId);
};

// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Job/Job.h"
#include "FxMat/FxMaterial.h"
#include "Model/Mix/MixUpdateCycle.h"
#include <DataDrivenShaderPlatformInfo.h>

//#include "T_Maths_ThreeInputs.generated.h"

//////////////////////////////////////////////////////////////////////////
/// MAD
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_ThreeInputs : public FSH_Base
{
public:
	SHADER_USE_PARAMETER_STRUCT(FSH_ThreeInputs, FSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, Operand1)
		SHADER_PARAMETER_TEXTURE(Texture2D, Operand2)
		SHADER_PARAMETER_TEXTURE(Texture2D, Operand3)
	END_SHADER_PARAMETER_STRUCT()

	TEXTURE_ENGINE_DEFAULT_PERMUTATION;
	TEXTUREGRAPH_ENGINE_DEFAULT_COMPILATION_ENV;
};

DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Mad, FSH_ThreeInputs);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Lerp, FSH_ThreeInputs);

//////////////////////////////////////////////////////////////////////////
/// Helper C++ class
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API T_Maths_ThreeInputs
{
public:
	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static TiledBlobPtr				CreateMad(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Operand1, TiledBlobPtr Operand2, TiledBlobPtr Operand3);
	static TiledBlobPtr				CreateLerp(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Operand1, TiledBlobPtr Operand2, TiledBlobPtr Operand3);
};

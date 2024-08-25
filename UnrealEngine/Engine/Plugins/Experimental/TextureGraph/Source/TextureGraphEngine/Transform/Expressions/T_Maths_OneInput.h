// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Job/Job.h"
#include "FxMat/FxMaterial.h"
#include "Model/Mix/MixUpdateCycle.h"
#include <DataDrivenShaderPlatformInfo.h>

#include "T_Maths_OneInput.generated.h"

//////////////////////////////////////////////////////////////////////////
/// Basic one input maths op
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_MathsOp_OneInput : public FSH_Base
{
public:
	SHADER_USE_PARAMETER_STRUCT(FSH_MathsOp_OneInput, FSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, Input)
	END_SHADER_PARAMETER_STRUCT()

	TEXTURE_ENGINE_DEFAULT_PERMUTATION;
	TEXTUREGRAPH_ENGINE_DEFAULT_COMPILATION_ENV;
};

DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Sin, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Cos, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Tan, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_ASin, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_ACos, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_ATan, FSH_MathsOp_OneInput);

DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_ToRadians, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_ToDegrees, FSH_MathsOp_OneInput);

DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Abs, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Sqrt, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Square, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Cube, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Cbrt, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Exp, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Log2, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Log10, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Log, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Floor, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Ceil, FSH_MathsOp_OneInput);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Round, FSH_MathsOp_OneInput);


//////////////////////////////////////////////////////////////////////////
/// Trigonometric functions
//////////////////////////////////////////////////////////////////////////
UENUM(BlueprintType)
enum ETrigFunction : int
{
	Sin							UMETA(DisplayName = "Sine"),
	Cos							UMETA(DisplayName = "Cosine"),
	Tan							UMETA(DisplayName = "Tangent"),
	ASin						UMETA(DisplayName = "ArcSine"),
	ACos						UMETA(DisplayName = "ArcCosine"),
	ATan						UMETA(DisplayName = "ArcTangent"),
};

//////////////////////////////////////////////////////////////////////////
/// Helper C++ class
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API T_Maths_OneInput
{
public:
	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static TiledBlobPtr			CreateTrigonometry(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Input, ETrigFunction Function);
	static TiledBlobPtr			CreateToRadians(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Input);
	static TiledBlobPtr			CreateToDegrees(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Input);
	static TiledBlobPtr			CreateAbs(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Input);
	static TiledBlobPtr			CreateSqrt(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Input);
	static TiledBlobPtr			CreateSquare(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Input);
	static TiledBlobPtr			CreateCbrt(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Input);
	static TiledBlobPtr			CreateCube(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Input);
	static TiledBlobPtr			CreateExp(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Input);
	static TiledBlobPtr			CreateLog2(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Input);
	static TiledBlobPtr			CreateLog10(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Input);
	static TiledBlobPtr			CreateLog(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Input);
	static TiledBlobPtr			CreateFloor(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Input);
	static TiledBlobPtr			CreateCeil(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Input);
	static TiledBlobPtr			CreateRound(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Input);
};

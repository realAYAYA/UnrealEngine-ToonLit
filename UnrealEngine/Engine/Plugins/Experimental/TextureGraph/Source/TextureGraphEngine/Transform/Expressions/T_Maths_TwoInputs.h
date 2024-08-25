// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Job/Job.h"
#include "FxMat/FxMaterial.h"
#include "Model/Mix/MixUpdateCycle.h"
#include <DataDrivenShaderPlatformInfo.h>

#include "T_Maths_TwoInputs.generated.h"

//////////////////////////////////////////////////////////////////////////
/// Basic math op
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API FSH_BasicMathOp : public FSH_Base
{
public:
	SHADER_USE_PARAMETER_STRUCT(FSH_BasicMathOp, FSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, Operand1)
		SHADER_PARAMETER_TEXTURE(Texture2D, Operand2)
	END_SHADER_PARAMETER_STRUCT()

	TEXTURE_ENGINE_DEFAULT_PERMUTATION;
	TEXTUREGRAPH_ENGINE_DEFAULT_COMPILATION_ENV;
};

DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Multiply, FSH_BasicMathOp);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Divide, FSH_BasicMathOp);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Add, FSH_BasicMathOp);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Subtract, FSH_BasicMathOp);

DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Dot, FSH_BasicMathOp);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Cross, FSH_BasicMathOp);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_Pow, FSH_BasicMathOp);

//////////////////////////////////////////////////////////////////////////
/// Comparison ops
//////////////////////////////////////////////////////////////////////////
UENUM(BlueprintType)
enum EIfThenElseOperator : int 
{
	GT		UMETA(DisplayName = "Greater Than"),
	GTE		UMETA(DisplayName = "Greater Than Or Equal To"),
	LT		UMETA(DisplayName = "Less Than"),
	LTE		UMETA(DisplayName = "Less Than Or Equal To"),
	EQ		UMETA(DisplayName = "Equal To (Tolerance = 0.00001)"),
	NEQ		UMETA(DisplayName = "Not Equal To (Tolerance = 0.00001)"),
};

UENUM(BlueprintType)
enum EIfThenElseType : int 
{
	IndividualComponent	UMETA(DisplayName = "Individual Components"),
	AllComponents		UMETA(DisplayName = "All Components"),
	Grayscale			UMETA(DisplayName = "Grayscale"),
};

class TEXTUREGRAPHENGINE_API FSH_IfThenElse : public FSH_Base
{
public:
	SHADER_USE_PARAMETER_STRUCT(FSH_IfThenElse, FSH_Base);
	DECLARE_GLOBAL_SHADER(FSH_IfThenElse);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, LHS)
		SHADER_PARAMETER_TEXTURE(Texture2D, RHS)
		SHADER_PARAMETER_TEXTURE(Texture2D, Then)
		SHADER_PARAMETER_TEXTURE(Texture2D, Else)
	END_SHADER_PARAMETER_STRUCT()

	static bool						ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}
	static void						ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& params, FShaderCompilerEnvironment& env) {}
};

DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_GT_Component, FSH_IfThenElse);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_GT_All, FSH_IfThenElse);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_GT_Grayscale, FSH_IfThenElse);

DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_GTE_Component, FSH_IfThenElse);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_GTE_All, FSH_IfThenElse);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_GTE_Grayscale, FSH_IfThenElse);

DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_LT_Component, FSH_IfThenElse);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_LT_All, FSH_IfThenElse);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_LT_Grayscale, FSH_IfThenElse);

DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_LTE_Component, FSH_IfThenElse);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_LTE_All, FSH_IfThenElse);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_LTE_Grayscale, FSH_IfThenElse);

DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_EQ_Component, FSH_IfThenElse);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_EQ_All, FSH_IfThenElse);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_EQ_Grayscale, FSH_IfThenElse);

DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_NEQ_Component, FSH_IfThenElse);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_NEQ_All, FSH_IfThenElse);
DECLARE_EMPTY_GLOBAL_SHADER_DERIVED_FROM(FSH_IfThenElse_NEQ_Grayscale, FSH_IfThenElse);

//////////////////////////////////////////////////////////////////////////
/// Helper C++ class
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API T_Maths_TwoInputs
{
public:
	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static TiledBlobPtr				CreateMultiply(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Operand1, TiledBlobPtr Operand2);
	static TiledBlobPtr				CreateDivide(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Operand1, TiledBlobPtr Operand2);
	static TiledBlobPtr				CreateAdd(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Operand1, TiledBlobPtr Operand2);
	static TiledBlobPtr				CreateSubtract(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Operand1, TiledBlobPtr Operand2);
	static TiledBlobPtr				CreateDot(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Operand1, TiledBlobPtr Operand2);
	static TiledBlobPtr				CreateCross(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Operand1, TiledBlobPtr Operand2);
	static TiledBlobPtr				CreatePow(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId, TiledBlobPtr Operand1, TiledBlobPtr Operand2);
	
	static TiledBlobPtr				CreateIfThenElse(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, int32 TargetId,
		TiledBlobPtr LHS, TiledBlobPtr RHS, TiledBlobPtr Then, TiledBlobPtr Else, EIfThenElseOperator Operator, EIfThenElseType Type);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAnimNextInterface.h"
#include "Units/RigUnit.h"
#include "AnimNextInterfaceGraph/Internal/AnimNextInterfaceExecuteContext.h"
#include "RigUnit_AnimNextInterfaceParameter.generated.h"

struct FAnimNextInterfaceUnitContext;
class UAnimNextInterfaceGraph;

/** Unit for reading parameters from context */
USTRUCT(meta = (DisplayName = "Parameter", Category = "Parameters", TitleColor = "1 0 0", NodeColor = "1 1 1"))
struct FRigUnit_AnimNextInterfaceParameter : public FRigUnit_AnimNextInterfaceBase
{
	GENERATED_BODY()

protected:
	static bool GetParameterInternal(FName InName, const FAnimNextInterfaceExecuteContext& InContext, void* OutResult);

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Input))
	FName Parameter = NAME_None;
};

/** Unit for reading float parameter from context */
USTRUCT(meta = (DisplayName = "Float Parameter", Category = "Parameters", TitleColor = "1 0 0", NodeColor = "1 1 1"))
struct FRigUnit_AnimNextInterfaceParameter_Float : public FRigUnit_AnimNextInterfaceParameter
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();
	
protected:
	UPROPERTY(EditAnywhere, Category = "Result", meta = (Output))
	float Result = 0.0f;
};

/** Unit for reading anim interface graph parameter from context */
USTRUCT(meta = (DisplayName = "Anim Interface Parameter", Category = "Parameters", TitleColor = "1 0 0", NodeColor = "1 1 1"))
struct FRigUnit_AnimNextInterfaceParameter_AnimNextInterface : public FRigUnit_AnimNextInterfaceParameter
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();
	
protected:
	UPROPERTY(EditAnywhere, Category = "Result", meta = (Output))
	TScriptInterface<IAnimNextInterface> Result = nullptr;
};

USTRUCT(BlueprintType)
struct FAnimNextInterfaceParameter
{
	GENERATED_BODY()

	FAnimNextInterfaceParameter()
	: Name(NAME_None)
	{}

	UPROPERTY(EditAnywhere, Category = "Anim Interface")
	FName Name;
};

USTRUCT(BlueprintType)
struct FAnimNextInterfaceParameter_Float
{
	GENERATED_BODY()
	
	FAnimNextInterfaceParameter_Float()
		: Name(NAME_None)
		, Value(0.f)
	{}

	UPROPERTY(EditAnywhere, Category = "Anim Interface")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Anim Interface")
	float Value;
};

USTRUCT(BlueprintType)
struct FAnimNextInterfaceParameter_Int
{
	GENERATED_BODY()

	FAnimNextInterfaceParameter_Int()
		: Name(NAME_None)
		, Value(0)
	{}

	UPROPERTY(EditAnywhere, Category = "Anim Interface")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Anim Interface")
	int32 Value;
};

USTRUCT(BlueprintType)
struct FAnimNextInterfaceParameter_Bool
{
	GENERATED_BODY()

	FAnimNextInterfaceParameter_Bool()
		: Name(NAME_None)
		, bValue(false)
	{}

	UPROPERTY(EditAnywhere, Category = "Anim Interface")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Anim Interface")
	bool bValue;
};

USTRUCT(BlueprintType)
struct FAnimNextInterfaceParameters1
{
	GENERATED_BODY()

	FAnimNextInterfaceParameters1()
		: Param0()
		, FloatParam(0.f)
	{}

	UPROPERTY(EditAnywhere, Category = "Anim Interface")
	FAnimNextInterfaceParameter_Int Param0;

	UPROPERTY(EditAnywhere, Category = "Anim Interface")
	float FloatParam;
};

USTRUCT(BlueprintType)
struct FAnimNextInterfaceParameter_AnimNextInterface : public FAnimNextInterfaceParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Anim Interface")
	TScriptInterface<IAnimNextInterface> Value = nullptr;
};

/** Base unit for calling anim interfaces from graphs */
USTRUCT(meta = (DisplayName = "Anim Interface", Category = "Execution", TitleColor = "1 0 0", NodeColor = "1 1 1"))
struct FRigUnit_AnimNextInterface : public FRigUnit_AnimNextInterfaceBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Anim Interface", meta = (Input))
	TScriptInterface<IAnimNextInterface> AnimNextInterface = nullptr;
};

/** Unit for getting a float via an anim interface */
USTRUCT(meta = (DisplayName = "Anim Interface Float", Category = "Parameters", TitleColor = "1 0 0", NodeColor = "1 1 1"))
struct FRigUnit_AnimNextInterface_Float : public FRigUnit_AnimNextInterface
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();
	
protected:
	UPROPERTY(EditAnywhere, Category = "Anim Interface", meta = (Input))
	TArray<FAnimNextInterfaceParameter_AnimNextInterface> Parameters;
	
	UPROPERTY(EditAnywhere, Category = "Result", meta = (Output))
	float Result = 0.0f;
};

/** Unit for getting a pose via an anim interface */
/*
USTRUCT(meta=(DisplayName="Get Pose"))
struct FRigUnit_AnimNextInterface_Pose : public FRigUnit
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();
	
protected:
	UPROPERTY(EditAnywhere, Category = "Pose Interface", meta = (Input))
	TScriptInterface<IAnimNextInterface> PoseInterface = nullptr;

	UPROPERTY(EditAnywhere, Category = "Pose Interface", meta = (Input))
	TArray<FAnimNextInterfaceParameter_AnimNextInterface> Parameters;

	UPROPERTY(EditAnywhere, Transient, DisplayName = "Result", Category = "BeginExecution", meta = (Output))
	FAnimNextInterfaceExecuteContext Result;
};
*/
/** Unit for getting a pose via an anim interface */
USTRUCT(meta = (DisplayName = "Float Operator", Category = "Operators", TitleColor = "1 0 0", NodeColor = "1 1 1"))
struct FRigUnit_FloatOperator : public FRigUnit_AnimNextInterfaceBase
{
	GENERATED_BODY()
	
	FRigUnit_FloatOperator()
		: ParamA(0.f)
		, ParamB(0.f)
		, Result(0.f)
	{}

	RIGVM_METHOD()
	void Execute();
	
protected:
	UPROPERTY(EditAnywhere, Category = "Pose Interface", meta = (Input))
	TScriptInterface<IAnimNextInterface> Operator = nullptr;

	UPROPERTY(EditAnywhere, Category = "Pose Interface", meta = (Input))
	float ParamA;

	UPROPERTY(EditAnywhere, Category = "Pose Interface", meta = (Input))
	float ParamB;

	UPROPERTY(EditAnywhere, Transient, DisplayName = "Result", Category = "BeginExecution", meta = (Output))
	float Result;
};

/** Unit for getting a pose via an anim interface */
USTRUCT(meta = (DisplayName = "Pose Operator", Category = "Operators", TitleColor = "1 0 0", NodeColor = "1 1 1"))
struct FRigUnit_PoseOperator : public FRigUnit_AnimNextInterfaceBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();
	
protected:
	UPROPERTY(EditAnywhere, Category = "Pose Interface", meta = (Input))
	TScriptInterface<IAnimNextInterface> Operator = nullptr;

	UPROPERTY(EditAnywhere, Category = "Pose Interface", meta = (Input))
	TArray<FRigVMExecuteContext> InputPoses;

	UPROPERTY(EditAnywhere, Category = "Pose Interface", meta = (Input))
	FAnimNextInterfaceParameters1 Parameters;

	UPROPERTY(EditAnywhere, Transient, DisplayName = "Result", Category = "BeginExecution", meta = (Output))
	FAnimNextInterfaceExecuteContext Result;
};

USTRUCT(BlueprintType)
struct FAnimSequenceParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Anim Sequence")
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Anim Sequence")
	float StartPosition = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Anim Sequence")
	bool bLoop = 0.0f;
};

/** Unit for getting a pose via an anim interface */
USTRUCT(meta = (DisplayName = "Anim Sequence", Category = "Animation", TitleColor = "1 0 0", NodeColor = "1 1 1"))
struct FRigUnit_AnimNextInterface_SequencePlayer : public FRigUnit_AnimNextInterfaceBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();
	
protected:
	UPROPERTY(EditAnywhere, Category = "Anim Interface", meta = (Input))
	FAnimSequenceParameters Parameters;

	UPROPERTY(EditAnywhere, Category = "Anim Interface", meta = (Input))
	TScriptInterface<IAnimNextInterface> Sequence = nullptr;
	
	UPROPERTY(EditAnywhere, Transient, DisplayName = "Result", Category = "BeginExecution", meta = (Output))
	FAnimNextInterfaceExecuteContext Result;
};

/** Unit for getting a pose via an anim interface */
USTRUCT(meta = (DisplayName = "Test Float State - Spring Damper Smoothing", Varying, Category = "Animation", TitleColor = "1 0 0", NodeColor = "1 1 1"))
struct FRigUnit_TestFloatState : public FRigUnit_AnimNextInterfaceBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();
	
protected:
	UPROPERTY(EditAnywhere, Category = "Parameters", meta=(Input))
	float TargetValue = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta=(Input))
	float TargetValueRate = 1.0f;
	
	UPROPERTY(EditAnywhere, Category = "Parameters", meta=(Input))
	float SmoothingTime = 1.0f;
	
	UPROPERTY(EditAnywhere, Category = "Parameters", meta=(Input))
	float DampingRatio = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta=(Output))
	float Result = 0.0f;
};

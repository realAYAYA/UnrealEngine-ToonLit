// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDataInterface.h"
#include "Units/RigUnit.h"
#include "RigUnit_DataInterfaceParameter.generated.h"

struct FDataInterfaceUnitContext;
class UDataInterfaceGraph;

/** Unit for reading parameters from context */
USTRUCT(meta=(DisplayName="Parameter"))
struct FRigUnit_DataInterfaceParameter : public FRigUnit
{
	GENERATED_BODY()

protected:
	static bool GetParameterInternal(FName InName, const FDataInterfaceUnitContext& InContext, void* OutResult);

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Input))
	FName Parameter = NAME_None;
};

/** Unit for reading float parameter from context */
USTRUCT(meta=(DisplayName="Float Parameter"))
struct FRigUnit_DataInterfaceParameter_Float : public FRigUnit_DataInterfaceParameter
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
	
protected:
	UPROPERTY(EditAnywhere, Category = "Result", meta = (Output))
	float Result = 0.0f;
};

/** Unit for reading data interface graph parameter from context */
USTRUCT(meta=(DisplayName="Data Interface Parameter"))
struct FRigUnit_DataInterfaceParameter_DataInterface : public FRigUnit_DataInterfaceParameter
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
	
protected:
	UPROPERTY(EditAnywhere, Category = "Result", meta = (Output))
	TScriptInterface<IDataInterface> Result = nullptr;
};

USTRUCT(BlueprintType)
struct FDataInterfaceParameter
{
	GENERATED_BODY()

	FDataInterfaceParameter()
	: Name(NAME_None)
	{}

	UPROPERTY(EditAnywhere, Category = "Data Interface")
	FName Name;
};

USTRUCT(BlueprintType)
struct FDataInterfaceParameter_Float
{
	GENERATED_BODY()
	
	FDataInterfaceParameter_Float()
		: Name(NAME_None)
		, Value(0.f)
	{}

	UPROPERTY(EditAnywhere, Category = "Data Interface")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Data Interface")
	float Value;
};

USTRUCT(BlueprintType)
struct FDataInterfaceParameter_Int
{
	GENERATED_BODY()

	FDataInterfaceParameter_Int()
		: Name(NAME_None)
		, Value(0)
	{}

	UPROPERTY(EditAnywhere, Category = "Data Interface")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Data Interface")
	int32 Value;
};

USTRUCT(BlueprintType)
struct FDataInterfaceParameter_Bool
{
	GENERATED_BODY()

	FDataInterfaceParameter_Bool()
		: Name(NAME_None)
		, bValue(false)
	{}

	UPROPERTY(EditAnywhere, Category = "Data Interface")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Data Interface")
	bool bValue;
};

USTRUCT(BlueprintType)
struct FDataInterfaceParameters1
{
	GENERATED_BODY()

	FDataInterfaceParameters1()
		: Param0()
		, FloatParam(0.f)
	{}

	UPROPERTY(EditAnywhere, Category = "Data Interface")
	FDataInterfaceParameter_Int Param0;

	UPROPERTY(EditAnywhere, Category = "Data Interface")
	float FloatParam;
};

USTRUCT(BlueprintType)
struct FDataInterfaceParameter_DataInterface : public FDataInterfaceParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Data Interface")
	TScriptInterface<IDataInterface> Value = nullptr;
};

/** Base unit for calling data interfaces from graphs */
USTRUCT(meta=(DisplayName="Data Interface"))
struct FRigUnit_DataInterface : public FRigUnit
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Data Interface", meta = (Input))
	TScriptInterface<IDataInterface> DataInterface = nullptr;
};

/** Unit for getting a float via a data interface */
USTRUCT(meta=(DisplayName="Data Interface Float"))
struct FRigUnit_DataInterface_Float : public FRigUnit_DataInterface
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
	
protected:
	UPROPERTY(EditAnywhere, Category = "Data Interface", meta = (Input))
	TArray<FDataInterfaceParameter_DataInterface> Parameters;
	
	UPROPERTY(EditAnywhere, Category = "Result", meta = (Output))
	float Result = 0.0f;
};

/** Unit for getting a pose via a data interface */
/*
USTRUCT(meta=(DisplayName="Get Pose"))
struct FRigUnit_DataInterface_Pose : public FRigUnit
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
	
protected:
	UPROPERTY(EditAnywhere, Category = "Pose Interface", meta = (Input))
	TScriptInterface<IDataInterface> PoseInterface = nullptr;

	UPROPERTY(EditAnywhere, Category = "Pose Interface", meta = (Input))
	TArray<FDataInterfaceParameter_DataInterface> Parameters;

	UPROPERTY(EditAnywhere, Transient, DisplayName = "Result", Category = "BeginExecution", meta = (Output))
	FDataInterfaceExecuteContext Result;
};
*/
/** Unit for getting a pose via a data interface */
USTRUCT(meta=(DisplayName="Float Operator"))
struct FRigUnit_FloatOperator : public FRigUnit
{
	GENERATED_BODY()
	
	FRigUnit_FloatOperator()
		: ParamA(0.f)
		, ParamB(0.f)
		, Result(0.f)
	{}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
	
protected:
	UPROPERTY(EditAnywhere, Category = "Pose Interface", meta = (Input))
	TScriptInterface<IDataInterface> Operator = nullptr;

	UPROPERTY(EditAnywhere, Category = "Pose Interface", meta = (Input))
	float ParamA;

	UPROPERTY(EditAnywhere, Category = "Pose Interface", meta = (Input))
	float ParamB;

	UPROPERTY(EditAnywhere, Transient, DisplayName = "Result", Category = "BeginExecution", meta = (Output))
	float Result;
};

/** Unit for getting a pose via a data interface */
USTRUCT(meta=(DisplayName="Pose Operator"))
struct FRigUnit_PoseOperator : public FRigUnit
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
	
protected:
	UPROPERTY(EditAnywhere, Category = "Pose Interface", meta = (Input))
	TScriptInterface<IDataInterface> Operator = nullptr;

	UPROPERTY(EditAnywhere, Category = "Pose Interface", meta = (Input))
	TArray<FRigVMExecuteContext> InputPoses;

	UPROPERTY(EditAnywhere, Category = "Pose Interface", meta = (Input))
	FDataInterfaceParameters1 Parameters;

	UPROPERTY(EditAnywhere, Transient, DisplayName = "Result", Category = "BeginExecution", meta = (Output))
	FRigVMExecuteContext Result;
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

/** Unit for getting a pose via a data interface */
USTRUCT(meta=(DisplayName="Anim Sequence"))
struct FRigUnit_DataInterface_SequencePlayer : public FRigUnit
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
	
protected:
	UPROPERTY(EditAnywhere, Category = "Data Interface", meta = (Input))
	FAnimSequenceParameters Parameters;

	UPROPERTY(EditAnywhere, Category = "Data Interface", meta = (Input))
	TScriptInterface<IDataInterface> Sequence = nullptr;
	
	UPROPERTY(EditAnywhere, Transient, DisplayName = "Result", Category = "BeginExecution", meta = (Output))
	FRigVMExecuteContext Result;
};

/** Unit for getting a pose via a data interface */
USTRUCT(meta=(DisplayName="Test Float State - Spring Damper Smoothing", Varying))
struct FRigUnit_TestFloatState : public FRigUnit
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
	
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
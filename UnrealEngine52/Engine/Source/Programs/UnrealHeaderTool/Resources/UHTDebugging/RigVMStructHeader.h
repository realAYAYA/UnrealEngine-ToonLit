// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "RigVMCore/RigVMExecuteContext.h"

#include "RigVMStructHeader.generated.h"

UENUM()
enum class ERigVMTestEnum : uint8
{
	A,
	B,
	C
};

UENUM()
namespace ERigVMTestNameSpaceEnum
{
	enum Type
	{
		A,
		B,
		C
	};
}

USTRUCT()
struct FRigVMStructBase
{
	GENERATED_BODY()
		
	UPROPERTY(meta = (Input))
	float Inherited;

	UPROPERTY(meta = (Output))
	float InheritedOutput;

	virtual FName GetNextAggregateName(const FName& InLastAggregateName) const {};

	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const {};
};

USTRUCT(BlueprintType)
struct FRigVMExecuteContext
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FUHTTestExecuteContext : public FRigVMExecuteContext
{
	GENERATED_BODY()
};

USTRUCT(meta = (Deprecated = "5.0.0", ExecuteContext = "FUHTTestExecuteContext"))
struct FRigVMMethodStruct : public FRigVMStructBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Clear();

	RIGVM_METHOD()
	virtual void Execute() override;

	RIGVM_METHOD()
	void Compute();

	UPROPERTY(meta = (Input))
	float A;

	UPROPERTY(meta = (Output))
	float B;

	UPROPERTY(meta = (Input))
	FVector C;

	UPROPERTY(meta = (Output))
	FVector D;

	UPROPERTY(meta = (Input, Output, ArraySize = 8))
	TArray<FVector> E;

	UPROPERTY(meta = (Input))
	TArray<FVector> F;

	UPROPERTY(meta = (Output, ArraySize = 8))
	TArray<FVector> G;

	UPROPERTY(meta = (ArraySize = 8))
	TArray<FVector> H;

	UPROPERTY()
	TArray<FVector> I;

	UPROPERTY()
	TArray<float> J;

	UPROPERTY()
	float Cache;

	UPROPERTY(meta = (Input))
	TEnumAsByte<ERigVMTestEnum> InputEnum;

	UPROPERTY()
	TEnumAsByte<ERigVMTestEnum> HiddenEnum;

	UPROPERTY(meta = (Input))
	TEnumAsByte<ERigVMTestNameSpaceEnum::Type> InputNameSpaceEnum;

	UPROPERTY()
	TEnumAsByte<ERigVMTestNameSpaceEnum::Type> HiddenNameSpaceEnum;

	RIGVM_METHOD()
	virtual FName GetNextAggregateName(const FName& InLastAggregatePinName) const override;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

USTRUCT(meta=(DisplayName="Control", Category="Controls", ShowVariableNameInTitle, Deprecated = "4.24.0"))
struct CONTROLRIG_API FRigUnit_Control
{
	GENERATED_BODY()

	FRigUnit_Control()
		: Factor(0)
	{
	}

	/** The transform of this control */
	UPROPERTY(EditAnywhere, Category="Control", meta=(Input))
	float Factor;
};

USTRUCT()
struct CONTROLRIG_API FRigUnit_Mutable
{
	GENERATED_BODY()

	FRigUnit_Mutable()
	{
	}

	RIGVM_METHOD()
	virtual void Execute();

	/** The transform of this control */
	UPROPERTY(EditAnywhere, Category="Control", meta=(Input, Output))
	FUHTTestExecuteContext ExecuteContext;
};

USTRUCT()
struct CONTROLRIG_API FRigUnit_BeginExecution
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	// The execution result
	UPROPERTY(EditAnywhere, Transient, DisplayName = "Execute", Category = "BeginExecution", meta = (Output))
	FUHTTestExecuteContext ExecuteContext;
};

USTRUCT()
struct FRigVMLazyStruct
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Compute();

	UPROPERTY(meta = (Input, Lazy))
	float A;

	UPROPERTY(meta = (Input, Lazy))
	TArray<float> B;

	UPROPERTY(meta = (Input))
	TArray<float> C;

	UPROPERTY(meta = (Output))
	float Result;
};

/**
 * Executes either the True or False branch based on the condition
 */
USTRUCT(meta = (DisplayName = "Branch", Keywords = "If"))
struct RIGVM_API FRigVMFunction_ControlFlowBranch
{
	GENERATED_BODY()

	FRigVMFunction_ControlFlowBranch()
	{
		Condition = false;
		BlockToRun = NAME_None;
	}

	RIGVM_METHOD()
	virtual void Execute();

	virtual const TArray<FName>& GetControlFlowBlocks_Impl() const override
	{
		static const TArray<FName> Blocks = {
			GET_MEMBER_NAME_CHECKED(FRigVMFunction_ControlFlowBranch, True),
			GET_MEMBER_NAME_CHECKED(FRigVMFunction_ControlFlowBranch, False),
			ForLoopCompletedPinName
		};
		return Blocks;
	}

	UPROPERTY(meta=(Input, DisplayName="Execute"))
	FRigVMExecuteContext ExecuteContext;

	UPROPERTY(meta=(Input))
	bool Condition;

	UPROPERTY(meta=(Output))
	FRigVMExecuteContext True ;

	UPROPERTY(meta=(Output))
	FRigVMExecuteContext False;

	UPROPERTY(meta=(Output))
	FRigVMExecuteContext Completed;

	UPROPERTY(meta=(Singleton))
	FName BlockToRun;
};

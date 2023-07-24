// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMStruct.h"
#include "RigVMFunction_ControlFlow.generated.h"

USTRUCT(meta=(Abstract, Category="Execution", NodeColor = "0, 0, 0, 1"))
struct RIGVM_API FRigVMFunction_ControlFlowBase : public FRigVMStruct
{
	GENERATED_BODY()
};

/**
 * Executes either the True or False branch based on the condition
 */
USTRUCT(meta = (DisplayName = "Branch", Keywords = "If"))
struct RIGVM_API FRigVMFunction_ControlFlowBranch : public FRigVMFunction_ControlFlowBase
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
	FRigVMExecuteContext True;

	UPROPERTY(meta=(Output))
	FRigVMExecuteContext False;

	UPROPERTY(meta=(Output))
	FRigVMExecuteContext Completed;

	UPROPERTY(meta=(Singleton))
	FName BlockToRun;
};

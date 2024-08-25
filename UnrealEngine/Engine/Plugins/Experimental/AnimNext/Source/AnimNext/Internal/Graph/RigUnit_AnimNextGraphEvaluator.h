// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextExecuteContext.h"

#include "RigUnit_AnimNextGraphEvaluator.generated.h"

// Represents an argument to a FRigUnit_AnimNextGraphEvaluator entry point
USTRUCT()
struct FAnimNextGraphEvaluatorExecuteArgument
{
	GENERATED_BODY();

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString CPPType;
};

// Represents an entry point of FRigUnit_AnimNextGraphEvaluator
USTRUCT()
struct FAnimNextGraphEvaluatorExecuteDefinition
{
	GENERATED_BODY();

	UPROPERTY()
	uint32 Hash = 0;

	UPROPERTY()
	FString MethodName;

	UPROPERTY()
	TArray<FAnimNextGraphEvaluatorExecuteArgument> Arguments;
};

/**
 * Animation graph evaluator
 * This node is only used at runtime.
 * It performs the animation graph update and evaluation through the data provided in the execution context.
 * It also holds all latent/lazy pins that the graph references in the editor.
 */
USTRUCT(meta=(DisplayName="Animation Runtime Output", Category="Events", NodeColor="1, 0, 0"))
struct ANIMNEXT_API FRigUnit_AnimNextGraphEvaluator : public FRigUnit_AnimNextBase
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input, Output))
	FAnimNextExecuteContext ExecuteContext;

	static void StaticExecute(FRigVMExtendedExecuteContext& RigVMExecuteContext, FRigVMMemoryHandleArray RigVMMemoryHandles, FRigVMPredicateBranchArray RigVMBranches);

	// The graph evaluator entry point is dynamically registered because it can have any number of latent pins with their own name/type
	// RigVM requires a mapping between argument list and method name and so we register things manually
	static void RegisterExecuteMethod(const FAnimNextGraphEvaluatorExecuteDefinition& ExecuteDefinition);

	// Returns an existing execute method, if found. Otherwise, nullptr is returned.
	static const FAnimNextGraphEvaluatorExecuteDefinition* FindExecuteMethod(uint32 ExecuteMethodHash);
};

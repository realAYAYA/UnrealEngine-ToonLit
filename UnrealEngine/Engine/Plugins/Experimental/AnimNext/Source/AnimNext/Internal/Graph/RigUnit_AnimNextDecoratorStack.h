// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextExecuteContext.h"
#include "DecoratorBase/DecoratorHandle.h"

#include "RigUnit_AnimNextDecoratorStack.generated.h"

/**
 * Animation graph decorator container
 * It contains a stack of decorators which are used during compilation to output a cooked graph
 * This node is synthetic and only present in the editor. During compilation, it is disconnected from
 * the graph and replaced by the cooked decorator metadata.
 */
USTRUCT(meta=(DisplayName="Decorator Stack", Category="Animation", NodeColor="0, 1, 1", Keywords="Decorator,Stack"))
struct ANIMNEXT_API FRigUnit_AnimNextDecoratorStack : public FRigUnit_AnimNextBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();

	// The execution result
	UPROPERTY(EditAnywhere, Category = Result, meta = (Output))
	FAnimNextDecoratorHandle Result;
};

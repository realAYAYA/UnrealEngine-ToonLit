// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextExecuteContext.h"
#include "DecoratorBase/DecoratorHandle.h"

#include "RigUnit_AnimNextGraphRoot.generated.h"

/**
 * Animation graph output
 * This is a synthetic node that represents the entry point for an animation graph for RigVM.
 * The graph editor will see this as the graph output in which to hook up the first animation node
 * to evaluate.
 * This node isn't used at runtime.
 */
USTRUCT(meta=(DisplayName="Animation Output", Category="Events", NodeColor="1, 0, 0", Keywords="Root,Output"))
struct ANIMNEXT_API FRigUnit_AnimNextGraphRoot : public FRigUnit_AnimNextBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void DummyExecute();

	virtual FName GetEventName() const override { return EventName; }
	virtual FString GetUnitSubTitle() const override { return EntryPoint.ToString(); };
	virtual bool CanOnlyExistOnce() const override { return true; }

	// The execution result
	UPROPERTY(EditAnywhere, Category = Result, meta = (Input))
	FAnimNextDecoratorHandle Result;

	// In order for this node to be considered an executable RigUnit, it needs a pin to derive from FRigVMExecuteContext
	// We keep it hidden it since we don't need it
	UPROPERTY()
	FAnimNextExecuteContext ExecuteContext;

	// The name of the entry point
	UPROPERTY(VisibleAnywhere, Category = Result, meta = (Hidden))
	FName EntryPoint = DefaultEntryPoint;

	// This unit is our graph entry point, it needs an event so we can call it
	static FName EventName;

	// Default entry point name
	static FName DefaultEntryPoint;
};

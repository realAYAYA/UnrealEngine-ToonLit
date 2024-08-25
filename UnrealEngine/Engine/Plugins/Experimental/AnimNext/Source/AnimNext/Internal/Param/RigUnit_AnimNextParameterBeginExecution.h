// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextParameterExecuteContext.h"
#include "RigUnit_AnimNextParameterBeginExecution.generated.h"

/**
 * Event for driving the skeleton hierarchy with variables and rig elements
 */
USTRUCT(meta=(DisplayName="Execute", Category="Events", NodeColor="1, 0, 0", Keywords="Begin,Update,Tick,Forward,Event"))
struct ANIMNEXT_API FRigUnit_AnimNextParameterBeginExecution : public FRigUnit_AnimNextParameterBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();

	virtual FString GetUnitLabel() const override { return EntryPoint.ToString(); };
	virtual FName GetEventName() const override { return EntryPoint; }
	virtual bool CanOnlyExistOnce() const override { return true; }

	// The execution result
	UPROPERTY(EditAnywhere, DisplayName = "Execute", Category = "Entry Point", meta = (Output))
	FAnimNextParameterExecuteContext ExecuteContext;

	// The name of the entry point
	UPROPERTY(VisibleAnywhere, Category = "Entry Point", meta = (Hidden))
	FName EntryPoint = DefaultEntryPoint;

	static FName DefaultEntryPoint;
};

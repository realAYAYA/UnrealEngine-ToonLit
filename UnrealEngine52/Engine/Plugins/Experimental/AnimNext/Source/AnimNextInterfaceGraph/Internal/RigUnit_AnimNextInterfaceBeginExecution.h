// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "AnimNextInterfaceExecuteContext.h"
#include "RigUnit_AnimNextInterfaceBeginExecution.generated.h"

/**
 * Event for driving the skeleton hierarchy with variables and rig elements
 */
USTRUCT(meta=(DisplayName="Execute Anim Interface", Category="Events", NodeColor="1, 0, 0", Keywords="Begin,Update,Tick,Forward,Event"))
struct ANIMNEXTINTERFACEGRAPH_API FRigUnit_AnimNextInterfaceBeginExecution : public FRigUnit_AnimNextInterfaceBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();

	virtual FName GetEventName() const override { return EventName; }
	virtual bool CanOnlyExistOnce() const override { return true; }

	// The execution result
	UPROPERTY(EditAnywhere, DisplayName = "Execute", Category = "BeginExecution", meta = (Output))
	FAnimNextInterfaceExecuteContext ExecuteContext;

	static FName EventName;
};

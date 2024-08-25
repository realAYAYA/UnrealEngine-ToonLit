// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "ControlRigDefines.h"
#include "RigUnit_BeginExecution.generated.h"

/**
 * Event for driving the skeleton hierarchy with variables and rig elements
 */
USTRUCT(meta=(DisplayName="Forwards Solve", Category="Events", NodeColor="1, 0, 0", Keywords="Begin,Update,Tick,Forward,Event"))
struct CONTROLRIG_API FRigUnit_BeginExecution : public FRigUnit
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	virtual FName GetEventName() const override { return EventName; }
	virtual bool CanOnlyExistOnce() const override { return true; }

	// The execution result
	UPROPERTY(EditAnywhere, Transient, DisplayName = "Execute", Category = "BeginExecution", meta = (Output))
	FControlRigExecuteContext ExecuteContext;

	static FName EventName;
};

/**
 * Event always executed before the forward solve
 */
USTRUCT(meta=(DisplayName="Pre Forwards Solve", Category="Events", NodeColor="1, 0, 0", Keywords="Begin,Update,Tick,PreForward,Event"))
struct CONTROLRIG_API FRigUnit_PreBeginExecution : public FRigUnit
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	virtual FName GetEventName() const override { return EventName; }
	virtual bool CanOnlyExistOnce() const override { return true; }

	// The execution result
	UPROPERTY(EditAnywhere, Transient, DisplayName = "Execute", Category = "BeginExecution", meta = (Output))
	FControlRigExecuteContext ExecuteContext;

	static FName EventName;
};

/**
 * Event always executed after the forward solve
 */
USTRUCT(meta=(DisplayName="Post Forwards Solve", Category="Events", NodeColor="1, 0, 0", Keywords="Begin,Update,Tick,PostForward,Event"))
struct CONTROLRIG_API FRigUnit_PostBeginExecution : public FRigUnit
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	virtual FName GetEventName() const override { return EventName; }
	virtual bool CanOnlyExistOnce() const override { return true; }

	// The execution result
	UPROPERTY(EditAnywhere, Transient, DisplayName = "Execute", Category = "BeginExecution", meta = (Output))
	FControlRigExecuteContext ExecuteContext;

	static FName EventName;
};

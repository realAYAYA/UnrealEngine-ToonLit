// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "ControlRigDefines.h"
#include "RigUnit_InteractionExecution.generated.h"

/**
 * Event for executing logic during an interaction
 */
USTRUCT(meta=(DisplayName="Interaction", Category="Events", NodeColor="1, 0, 0", Keywords="Manipulation,Event,During,Interacting"))
struct CONTROLRIG_API FRigUnit_InteractionExecution : public FRigUnit
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	virtual FName GetEventName() const override { return EventName; }

	// The execution result
	UPROPERTY(EditAnywhere, Transient, DisplayName = "Execute", Category = "BeginExecution", meta = (Output))
	FControlRigExecuteContext ExecuteContext;

	static FName EventName;
};

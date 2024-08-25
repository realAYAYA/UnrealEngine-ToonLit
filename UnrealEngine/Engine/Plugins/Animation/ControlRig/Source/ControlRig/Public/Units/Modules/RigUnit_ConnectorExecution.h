// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "ControlRigDefines.h"
#include "RigUnit_ConnectorExecution.generated.h"

/**
 * Event for filtering connection candidates
 */
USTRUCT(meta=(DisplayName="Connector", Category="Events", NodeColor="1, 0, 0", Keywords="Event,During,Resolve,Connect,Filter"))
struct CONTROLRIG_API FRigUnit_ConnectorExecution : public FRigUnit
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	virtual FName GetEventName() const override { return EventName; }

	// The execution result
	UPROPERTY(EditAnywhere, Transient, DisplayName = "Execute", Category = "BeginExecution", meta = (Output))
	FControlRigExecuteContext ExecuteContext;

	static FName EventName;
};

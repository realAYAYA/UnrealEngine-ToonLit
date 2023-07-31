// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "ControlRigDefines.h"
#include "RigUnit_UserDefinedEvent.generated.h"

/**
 * User Defined Event for running custom logic
 */
USTRUCT(meta=(DisplayName="User Defined Event", Category="Events", NodeColor="1, 0, 0", Keywords="Event,Entry,MyEvent"))
struct CONTROLRIG_API FRigUnit_UserDefinedEvent : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_UserDefinedEvent()
	{
		EventName = TEXT("My Event");
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	virtual FString GetUnitLabel() const override;
	virtual FName GetEventName() const override { return EventName; }
	virtual bool CanOnlyExistOnce() const override { return false; }

	// The execution result
	UPROPERTY(EditAnywhere, Transient, DisplayName = "Execute", Category = "UserDefinedEvent", meta = (Output))
	FControlRigExecuteContext ExecuteContext;

	// True if the current interaction is a rotation
	UPROPERTY(EditAnywhere, Transient, Category = "UserDefinedEvent", meta = (Input,Constant,DetailsOnly))
	FName EventName;
};

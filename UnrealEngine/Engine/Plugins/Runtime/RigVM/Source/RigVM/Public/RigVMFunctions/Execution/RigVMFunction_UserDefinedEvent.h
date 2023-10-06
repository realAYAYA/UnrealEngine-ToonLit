// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMStruct.h"
#include "RigVMFunction_UserDefinedEvent.generated.h"

/**
 * User Defined Event for running custom logic
 */
USTRUCT(meta=(DisplayName="User Defined Event", Category="Events", NodeColor="1, 0, 0", Keywords="Event,Entry,MyEvent"))
struct RIGVM_API FRigVMFunction_UserDefinedEvent : public FRigVMStruct
{
	GENERATED_BODY()

	FRigVMFunction_UserDefinedEvent()
	{
		EventName = TEXT("My Event");
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	virtual FString GetUnitLabel() const override;
	virtual FName GetEventName() const override { return EventName; }
	virtual bool CanOnlyExistOnce() const override { return false; }

	// The execution result
	UPROPERTY(EditAnywhere, Transient, DisplayName = "Execute", Category = "UserDefinedEvent", meta = (Output))
	FRigVMExecuteContext ExecuteContext;

	// True if the current interaction is a rotation
	UPROPERTY(EditAnywhere, Transient, Category = "UserDefinedEvent", meta = (Input,Constant,DetailsOnly))
	FName EventName;
};

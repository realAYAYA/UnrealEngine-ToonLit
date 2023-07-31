// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_SimBase.h"
#include "RigUnit_DeltaFromPrevious.generated.h"

/**
 * Computes the difference from the previous value going through the node
 */
USTRUCT(meta=(DisplayName="DeltaFromPrevious (Float)", Category = "Simulation|Time", TemplateName="DeltaFromPrevious", Keywords="Difference,Velocity,Acceleration"))
struct CONTROLRIG_API FRigUnit_DeltaFromPreviousFloat : public FRigUnit_SimBase
{
	GENERATED_BODY()
	
	FRigUnit_DeltaFromPreviousFloat()
	{
		Value = Delta = PreviousValue = Cache = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	float Value;

	UPROPERTY(meta=(Output))
	float Delta;

	UPROPERTY(meta=(Output))
	float PreviousValue;

	UPROPERTY()
	float Cache;
};

/**
 * Computes the difference from the previous value going through the node
 */
USTRUCT(meta=(DisplayName="DeltaFromPrevious (Vector)", Category = "Simulation|Time", TemplateName="DeltaFromPrevious", Keywords="Difference,Velocity,Acceleration"))
struct CONTROLRIG_API FRigUnit_DeltaFromPreviousVector : public FRigUnit_SimBase
{
	GENERATED_BODY()
	
	FRigUnit_DeltaFromPreviousVector()
	{
		Value = Delta = PreviousValue = Cache = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FVector Value;

	UPROPERTY(meta=(Output))
	FVector Delta;

	UPROPERTY(meta=(Output))
	FVector PreviousValue;

	UPROPERTY()
	FVector Cache;
};

/**
 * Computes the difference from the previous value going through the node
 */
USTRUCT(meta=(DisplayName="DeltaFromPrevious (Quaternion)", Category = "Simulation|Time", TemplateName="DeltaFromPrevious", Keywords="Difference,Velocity,Acceleration"))
struct CONTROLRIG_API FRigUnit_DeltaFromPreviousQuat : public FRigUnit_SimBase
{
	GENERATED_BODY()
	
	FRigUnit_DeltaFromPreviousQuat()
	{
		Value = Delta = PreviousValue = Cache = FQuat::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FQuat Value;

	UPROPERTY(meta=(Output))
	FQuat Delta;

	UPROPERTY(meta=(Output))
	FQuat PreviousValue;

	UPROPERTY()
	FQuat Cache;
};

/**
 * Computes the difference from the previous value going through the node
 */
USTRUCT(meta=(DisplayName="DeltaFromPrevious (Transform)", Category = "Simulation|Time", TemplateName="DeltaFromPrevious", Keywords="Difference,Velocity,Acceleration"))
struct CONTROLRIG_API FRigUnit_DeltaFromPreviousTransform : public FRigUnit_SimBase
{
	GENERATED_BODY()
	
	FRigUnit_DeltaFromPreviousTransform()
	{
		Value = Delta = PreviousValue = Cache = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FTransform Value;

	UPROPERTY(meta=(Output))
	FTransform Delta;

	UPROPERTY(meta=(Output))
	FTransform PreviousValue;

	UPROPERTY()
	FTransform Cache;
};

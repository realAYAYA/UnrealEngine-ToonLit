// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunctions/Simulation/RigVMFunction_SimBase.h"
#include "RigVMFunction_DeltaFromPrevious.generated.h"

/**
 * Computes the difference from the previous value going through the node
 */
USTRUCT(meta=(DisplayName="DeltaFromPrevious (Float)", Category = "Simulation|Time", TemplateName="DeltaFromPrevious", Keywords="Difference,Velocity,Acceleration"))
struct RIGVM_API FRigVMFunction_DeltaFromPreviousFloat : public FRigVMFunction_SimBase
{
	GENERATED_BODY()
	
	FRigVMFunction_DeltaFromPreviousFloat()
	{
		Value = Delta = PreviousValue = Cache = 0.f;
		bIsInitialized = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	float Value;

	UPROPERTY(meta=(Output))
	float Delta;

	UPROPERTY(meta=(Output))
	float PreviousValue;

	UPROPERTY()
	float Cache;

	UPROPERTY()
	bool bIsInitialized;
};

/**
 * Computes the difference from the previous value going through the node
 */
USTRUCT(meta=(DisplayName="DeltaFromPrevious (Vector)", Category = "Simulation|Time", TemplateName="DeltaFromPrevious", Keywords="Difference,Velocity,Acceleration"))
struct RIGVM_API FRigVMFunction_DeltaFromPreviousVector : public FRigVMFunction_SimBase
{
	GENERATED_BODY()
	
	FRigVMFunction_DeltaFromPreviousVector()
	{
		Value = Delta = PreviousValue = Cache = FVector::ZeroVector;
		bIsInitialized = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FVector Value;

	UPROPERTY(meta=(Output))
	FVector Delta;

	UPROPERTY(meta=(Output))
	FVector PreviousValue;

	UPROPERTY()
	FVector Cache;

	UPROPERTY()
	bool bIsInitialized;
};

/**
 * Computes the difference from the previous value going through the node
 */
USTRUCT(meta=(DisplayName="DeltaFromPrevious (Quaternion)", Category = "Simulation|Time", TemplateName="DeltaFromPrevious", Keywords="Difference,Velocity,Acceleration"))
struct RIGVM_API FRigVMFunction_DeltaFromPreviousQuat : public FRigVMFunction_SimBase
{
	GENERATED_BODY()
	
	FRigVMFunction_DeltaFromPreviousQuat()
	{
		Value = Delta = PreviousValue = Cache = FQuat::Identity;
		bIsInitialized = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FQuat Value;

	UPROPERTY(meta=(Output))
	FQuat Delta;

	UPROPERTY(meta=(Output))
	FQuat PreviousValue;

	UPROPERTY()
	FQuat Cache;

	UPROPERTY()
	bool bIsInitialized;
};

/**
 * Computes the difference from the previous value going through the node
 */
USTRUCT(meta=(DisplayName="DeltaFromPrevious (Transform)", Category = "Simulation|Time", TemplateName="DeltaFromPrevious", Keywords="Difference,Velocity,Acceleration"))
struct RIGVM_API FRigVMFunction_DeltaFromPreviousTransform : public FRigVMFunction_SimBase
{
	GENERATED_BODY()
	
	FRigVMFunction_DeltaFromPreviousTransform()
	{
		Value = Delta = PreviousValue = Cache = FTransform::Identity;
		bIsInitialized = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FTransform Value;

	UPROPERTY(meta=(Output))
	FTransform Delta;

	UPROPERTY(meta=(Output))
	FTransform PreviousValue;

	UPROPERTY()
	FTransform Cache;

	UPROPERTY()
	bool bIsInitialized;
};

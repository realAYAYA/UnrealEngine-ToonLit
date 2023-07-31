// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_SimBase.h"
#include "RigUnit_Kalman.generated.h"

/**
 * Averages a value over time.
 * This uses a Kalman Filter internally.
 */
USTRUCT(meta=(DisplayName="Average Over Time (Float)", Category = "Simulation|Time", TemplateName="KalmanFilter", Keywords="Average,Smooth,KalmanFilter"))
struct CONTROLRIG_API FRigUnit_KalmanFloat : public FRigUnit_SimBase
{
	GENERATED_BODY()
	
	FRigUnit_KalmanFloat()
	{
		Value = Result = 0.f;
		BufferSize = 16;
		LastInsertIndex = 0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	float Value;

	UPROPERTY(meta=(Input, Constant))
	int32 BufferSize;

	UPROPERTY(meta=(Output))
	float Result;

	UPROPERTY(meta=(ArraySize = "FMath::Clamp<int32>(BufferSize, 1, 512)"))
	TArray<float> Buffer;

	UPROPERTY()
	int32 LastInsertIndex;
};

/**
 * Averages a value over time.
 * This uses a Kalman Filter internally.
 */
USTRUCT(meta=(DisplayName="Average Over Time (Vector)", Category = "Simulation|Time", TemplateName="KalmanFilter", Keywords="Average,Smooth,KalmanFilter"))
struct CONTROLRIG_API FRigUnit_KalmanVector : public FRigUnit_SimBase
{
	GENERATED_BODY()
	
	FRigUnit_KalmanVector()
	{
		Value = Result = FVector::ZeroVector;
		BufferSize = 16;
		LastInsertIndex = 0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FVector Value;

	UPROPERTY(meta=(Input, Constant))
	int32 BufferSize;

	UPROPERTY(meta=(Output))
	FVector Result;

	UPROPERTY(meta=(ArraySize = "FMath::Clamp<int32>(BufferSize, 1, 512)"))
	TArray<FVector> Buffer;

	UPROPERTY()
	int32 LastInsertIndex;
};

/**
 * Averages a transform over time.
 * This uses a Kalman Filter internally.
 */
USTRUCT(meta=(DisplayName="Average Over Time (Transform)", Category = "Simulation|Time", TemplateName="KalmanFilter", Keywords="Average,Smooth,KalmanFilter"))
struct CONTROLRIG_API FRigUnit_KalmanTransform : public FRigUnit_SimBase
{
	GENERATED_BODY()
	
	FRigUnit_KalmanTransform()
	{
		Value = Result = FTransform::Identity;
		BufferSize = 16;
		LastInsertIndex = 0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FTransform Value;

	UPROPERTY(meta=(Input, Constant))
	int32 BufferSize;

	UPROPERTY(meta=(Output))
	FTransform Result;

	UPROPERTY(meta = (ArraySize = "FMath::Clamp<int32>(BufferSize, 1, 512)"))
	TArray<FTransform> Buffer;

	UPROPERTY()
	int32 LastInsertIndex;
};

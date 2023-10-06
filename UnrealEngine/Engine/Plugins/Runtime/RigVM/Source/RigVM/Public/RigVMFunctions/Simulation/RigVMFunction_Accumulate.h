// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunctions/Simulation/RigVMFunction_SimBase.h"
#include "RigVMFunction_Accumulate.generated.h"

USTRUCT(meta=(Category="Simulation|Accumulate"))
struct RIGVM_API FRigVMFunction_AccumulateBase : public FRigVMFunction_SimBase
{
	GENERATED_BODY()
};

/**
 * Adds a value over time over and over again
 */
USTRUCT(meta=(DisplayName="Accumulate Add (Float)", TemplateName="AccumulateAdd", Keywords="Simulate,++"))
struct RIGVM_API FRigVMFunction_AccumulateFloatAdd : public FRigVMFunction_AccumulateBase
{
	GENERATED_BODY()
	
	FRigVMFunction_AccumulateFloatAdd()
	{
		InitialValue = Increment = Result = AccumulatedValue = 0.f;
		bIntegrateDeltaTime = false;
		bIsInitialized = false;
	}

	virtual void Initialize() override { bIsInitialized = false; }

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	float Increment;

	UPROPERTY(meta = (Input))
	float InitialValue;

	UPROPERTY(meta=(Input))
	bool bIntegrateDeltaTime;

	UPROPERTY(meta=(Output))
	float Result;

	UPROPERTY()
	float AccumulatedValue;

	UPROPERTY()
	bool bIsInitialized;
};

/**
 * Adds a vector over time over and over again
 */
USTRUCT(meta = (DisplayName="Accumulate Add (Vector)", TemplateName="AccumulateAdd", Keywords="Simulate,++"))
struct RIGVM_API FRigVMFunction_AccumulateVectorAdd : public FRigVMFunction_AccumulateBase
{
	GENERATED_BODY()

	FRigVMFunction_AccumulateVectorAdd()
	{
		InitialValue = Increment = Result = AccumulatedValue = FVector::ZeroVector;
		bIntegrateDeltaTime = false;
		bIsInitialized = false;
	}

	virtual void Initialize() override { bIsInitialized = false; }

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FVector Increment;

	UPROPERTY(meta = (Input))
	FVector InitialValue;

	UPROPERTY(meta = (Input))
	bool bIntegrateDeltaTime;

	UPROPERTY(meta = (Output))
	FVector Result;

	UPROPERTY()
	FVector AccumulatedValue;

	UPROPERTY()
	bool bIsInitialized;
};

/**
 * Multiplies a value over time over and over again
 */
USTRUCT(meta=(DisplayName="Accumulate Mul (Float)", TemplateName="AccumulateMul", Keywords="Simulate,**"))
struct RIGVM_API FRigVMFunction_AccumulateFloatMul : public FRigVMFunction_AccumulateBase
{
	GENERATED_BODY()
	
	FRigVMFunction_AccumulateFloatMul()
	{
		InitialValue = Multiplier = Result = AccumulatedValue = 1.f;
		bIntegrateDeltaTime = false;
		bIsInitialized = false;
	}

	virtual void Initialize() override { bIsInitialized = false; }

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	float Multiplier;

	UPROPERTY(meta = (Input))
	float InitialValue;

	UPROPERTY(meta=(Input))
	bool bIntegrateDeltaTime;

	UPROPERTY(meta=(Output))
	float Result;

	UPROPERTY()
	float AccumulatedValue;

	UPROPERTY()
	bool bIsInitialized;
};

/**
 * Multiplies a vector over time over and over again
 */
USTRUCT(meta = (DisplayName="Accumulate Mul (Vector)", TemplateName="AccumulateMul", Keywords="Simulate,**"))
struct RIGVM_API FRigVMFunction_AccumulateVectorMul : public FRigVMFunction_AccumulateBase
{
	GENERATED_BODY()

	FRigVMFunction_AccumulateVectorMul()
	{
		InitialValue = Multiplier = Result = AccumulatedValue = FVector::OneVector;
		bIntegrateDeltaTime = false;
		bIsInitialized = false;
	}

	virtual void Initialize() override { bIsInitialized = false; }

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FVector Multiplier;

	UPROPERTY(meta = (Input))
	FVector InitialValue;

	UPROPERTY(meta = (Input))
	bool bIntegrateDeltaTime;

	UPROPERTY(meta = (Output))
	FVector Result;

	UPROPERTY()
	FVector AccumulatedValue;

	UPROPERTY()
	bool bIsInitialized;
};

/**
 * Multiplies a quaternion over time over and over again
 */
USTRUCT(meta=(DisplayName="Accumulate Mul (Quaternion)", TemplateName="AccumulateMul", Keywords="Simulate,**"))
struct RIGVM_API FRigVMFunction_AccumulateQuatMul : public FRigVMFunction_AccumulateBase
{
	GENERATED_BODY()
	
	FRigVMFunction_AccumulateQuatMul()
	{
		InitialValue = Multiplier = Result = AccumulatedValue = FQuat::Identity;
		bIntegrateDeltaTime = bFlipOrder = false;
		bIsInitialized = false;
	}

	virtual void Initialize() override { bIsInitialized = false; }

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FQuat Multiplier;

	UPROPERTY(meta = (Input))
	FQuat InitialValue;

	UPROPERTY(meta = (Input))
	bool bFlipOrder;

	UPROPERTY(meta=(Input))
	bool bIntegrateDeltaTime;

	UPROPERTY(meta=(Output))
	FQuat Result;

	UPROPERTY()
	FQuat AccumulatedValue;

	UPROPERTY()
	bool bIsInitialized;
};

/**
 * Multiplies a transform over time over and over again
 */
USTRUCT(meta = (DisplayName="Accumulate Mul (Transform)", TemplateName="AccumulateMul", Keywords="Simulate,**"))
struct RIGVM_API FRigVMFunction_AccumulateTransformMul : public FRigVMFunction_AccumulateBase
{
	GENERATED_BODY()

	FRigVMFunction_AccumulateTransformMul()
	{
		InitialValue = Multiplier = Result = AccumulatedValue = FTransform::Identity;
		bIntegrateDeltaTime = bFlipOrder = false;
		bIsInitialized = false;
	}

	virtual void Initialize() override { bIsInitialized = false; }

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FTransform Multiplier;

	UPROPERTY(meta = (Input))
	FTransform InitialValue;

	UPROPERTY(meta = (Input))
	bool bFlipOrder;

	UPROPERTY(meta = (Input))
	bool bIntegrateDeltaTime;

	UPROPERTY(meta = (Output))
	FTransform Result;

	UPROPERTY()
	FTransform AccumulatedValue;

	UPROPERTY()
	bool bIsInitialized;
};

/**
 * Interpolates two values over time over and over again
 */
USTRUCT(meta=(DisplayName="Accumulate Lerp (Float)", TemplateName="AccumulateLerp", Keywords="Simulate,Ramp"))
struct RIGVM_API FRigVMFunction_AccumulateFloatLerp : public FRigVMFunction_AccumulateBase
{
	GENERATED_BODY()
	
	FRigVMFunction_AccumulateFloatLerp()
	{
		InitialValue = TargetValue = Blend = Result = AccumulatedValue = 0.f;
		bIntegrateDeltaTime = false;
		bIsInitialized = false;
	}

	virtual void Initialize() override { bIsInitialized = false; }

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	float TargetValue;

	UPROPERTY(meta = (Input))
	float InitialValue;

	UPROPERTY(meta = (Input))
	float Blend;

	UPROPERTY(meta=(Input))
	bool bIntegrateDeltaTime;

	UPROPERTY(meta=(Output))
	float Result;

	UPROPERTY()
	float AccumulatedValue;

	UPROPERTY()
	bool bIsInitialized;
};

/**
 * Interpolates two vectors over time over and over again
 */
USTRUCT(meta = (DisplayName="Accumulate Lerp (Vector)", TemplateName="AccumulateLerp", Keywords="Simulate,Ramp"))
struct RIGVM_API FRigVMFunction_AccumulateVectorLerp : public FRigVMFunction_AccumulateBase
{
	GENERATED_BODY()

	FRigVMFunction_AccumulateVectorLerp()
	{
		InitialValue = TargetValue = Result = AccumulatedValue = FVector::ZeroVector;
		Blend = 0.f;
		bIntegrateDeltaTime = false;
		bIsInitialized = false;
	}

	virtual void Initialize() override { bIsInitialized = false; }

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FVector TargetValue;

	UPROPERTY(meta = (Input))
	FVector InitialValue;

	UPROPERTY(meta = (Input))
	float Blend;

	UPROPERTY(meta = (Input))
	bool bIntegrateDeltaTime;

	UPROPERTY(meta = (Output))
	FVector Result;

	UPROPERTY()
	FVector AccumulatedValue;

	UPROPERTY()
	bool bIsInitialized;
};

/**
 * Interpolates two quaternions over time over and over again
 */
USTRUCT(meta=(DisplayName="Accumulate Lerp (Quaternion)", TemplateName="AccumulateLerp", Keywords="Simulate,Ramp"))
struct RIGVM_API FRigVMFunction_AccumulateQuatLerp : public FRigVMFunction_AccumulateBase
{
	GENERATED_BODY()
	
	FRigVMFunction_AccumulateQuatLerp()
	{
		InitialValue = TargetValue = Result = AccumulatedValue = FQuat::Identity;
		Blend = 0.f;
		bIntegrateDeltaTime = false;
		bIsInitialized = false;
	}

	virtual void Initialize() override { bIsInitialized = false; }

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FQuat TargetValue;

	UPROPERTY(meta = (Input))
	FQuat InitialValue;

	UPROPERTY(meta = (Input))
	float Blend;

	UPROPERTY(meta=(Input))
	bool bIntegrateDeltaTime;

	UPROPERTY(meta=(Output))
	FQuat Result;

	UPROPERTY()
	FQuat AccumulatedValue;

	UPROPERTY()
	bool bIsInitialized;
};

/**
 * Interpolates two transforms over time over and over again
 */
USTRUCT(meta = (DisplayName="Accumulate Lerp (Transform)", TemplateName="AccumulateLerp", Keywords="Simulate,Ramp"))
struct RIGVM_API FRigVMFunction_AccumulateTransformLerp : public FRigVMFunction_AccumulateBase
{
	GENERATED_BODY()

	FRigVMFunction_AccumulateTransformLerp()
	{
		InitialValue = TargetValue = Result = AccumulatedValue = FTransform::Identity;
		Blend = 0.f;
		bIntegrateDeltaTime = false;
		bIsInitialized = false;
	}

	virtual void Initialize() override { bIsInitialized = false; }

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FTransform TargetValue;

	UPROPERTY(meta = (Input))
	FTransform InitialValue;

	UPROPERTY(meta = (Input))
	float Blend;

	UPROPERTY(meta = (Input))
	bool bIntegrateDeltaTime;

	UPROPERTY(meta = (Output))
	FTransform Result;

	UPROPERTY()
	FTransform AccumulatedValue;

	UPROPERTY()
	bool bIsInitialized;
};

/**
 * Accumulates the min and max values over time
 */
USTRUCT(meta=(DisplayName="Accumulate Range (Float)", TemplateName="AccumulateRange", Keywords="Range"))
struct RIGVM_API FRigVMFunction_AccumulateFloatRange : public FRigVMFunction_AccumulateBase
{
	GENERATED_BODY()
	
	FRigVMFunction_AccumulateFloatRange()
	{
		Value = Minimum = Maximum = AccumulatedMinimum = AccumulatedMaximum = 0.f;
		bIsInitialized = false;
	}

	virtual void Initialize() override { bIsInitialized = false; }

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	float Value;

	UPROPERTY(meta= (Output))
	float Minimum;

	UPROPERTY(meta= (Output))
	float Maximum;

	UPROPERTY()
	float AccumulatedMinimum;

	UPROPERTY()
	float AccumulatedMaximum;

	UPROPERTY()
	bool bIsInitialized;
};

/**
 * Accumulates the min and max values over time
 */
USTRUCT(meta = (DisplayName="Accumulate Range (Vector)", TemplateName="AccumulateRange", Keywords="Range"))
struct RIGVM_API FRigVMFunction_AccumulateVectorRange : public FRigVMFunction_AccumulateBase
{
	GENERATED_BODY()

	FRigVMFunction_AccumulateVectorRange()
	{
		Value = Minimum = Maximum = AccumulatedMinimum = AccumulatedMaximum = FVector::ZeroVector;
		bIsInitialized = false;
	}

	virtual void Initialize() override { bIsInitialized = false; }

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FVector Value;

	UPROPERTY(meta= (Output))
	FVector Minimum;

	UPROPERTY(meta= (Output))
	FVector Maximum;

	UPROPERTY()
	FVector AccumulatedMinimum;

	UPROPERTY()
	FVector AccumulatedMaximum;

	UPROPERTY()
	bool bIsInitialized;
};

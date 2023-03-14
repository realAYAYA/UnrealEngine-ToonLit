// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_SimBase.h"
#include "Animation/InputScaleBias.h"
#include "RigUnit_AlphaInterp.generated.h"

/**
 * Takes in a float value and outputs an accumulated value with a customized scale and clamp
 */
USTRUCT(meta=(DisplayName="Alpha Interpolate", Keywords="Alpha,Lerp,LinearInterpolate", Category = "Simulation|Time", TemplateName="AlphaInterp", MenuDescSuffix = "(Float)"))
struct CONTROLRIG_API FRigUnit_AlphaInterp : public FRigUnit_SimBase
{
	GENERATED_BODY()
	
	FRigUnit_AlphaInterp()
	{
		Value = Result = 0.f;
		ScaleBiasClamp = FInputScaleBiasClamp();
		bMapRange = ScaleBiasClamp.bMapRange;
		bClampResult = ScaleBiasClamp.bClampResult;
		bInterpResult = ScaleBiasClamp.bInterpResult;
		InRange = ScaleBiasClamp.InRange;
		OutRange = ScaleBiasClamp.OutRange;
		Scale = ScaleBiasClamp.Scale;
		Bias = ScaleBiasClamp.Bias;
		ClampMin = ScaleBiasClamp.ClampMin;
		ClampMax = ScaleBiasClamp.ClampMax;
		InterpSpeedIncreasing = ScaleBiasClamp.InterpSpeedIncreasing;
		InterpSpeedDecreasing = ScaleBiasClamp.InterpSpeedDecreasing;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	virtual FString ProcessPinLabelForInjection(const FString& InLabel) const override;

	UPROPERTY(meta=(Input))
	float Value;

	UPROPERTY(meta = (Input))
	float Scale;

	UPROPERTY(meta = (Input))
	float Bias;

	UPROPERTY(meta=(Input, Constant))
	bool bMapRange;

	UPROPERTY(meta=(Input, EditCondition = "bMapRange"))
	FInputRange InRange;

	UPROPERTY(meta=(Input, EditCondition = "bMapRange"))
	FInputRange OutRange;

	UPROPERTY(meta = (Input, Constant))
	bool bClampResult;

	UPROPERTY(meta=(Input, EditCondition = "bClampResult"))
	float ClampMin;

	UPROPERTY(meta=(Input, EditCondition = "bClampResult"))
	float ClampMax;

	UPROPERTY(meta = (Input, Constant))
	bool bInterpResult;

	UPROPERTY(meta=(Input, EditCondition = "bInterpResult"))
	float InterpSpeedIncreasing;

	UPROPERTY(meta=(Input, EditCondition = "bInterpResult"))
	float InterpSpeedDecreasing;

	UPROPERTY(meta=(Output))
	float Result;

	UPROPERTY()
	FInputScaleBiasClamp ScaleBiasClamp;
};

/**
 * Takes in a vector value and outputs an accumulated value with a customized scale and clamp
 */
USTRUCT(meta=(DisplayName="Alpha Interpolate", Keywords="Alpha,Lerp,LinearInterpolate", Category = "Simulation|Time", TemplateName="AlphaInterp", MenuDescSuffix = "(Vector)"))
struct CONTROLRIG_API FRigUnit_AlphaInterpVector : public FRigUnit_SimBase
{
	GENERATED_BODY()
	
	FRigUnit_AlphaInterpVector()
	{
		Value = Result = FVector::ZeroVector;
		ScaleBiasClamp = FInputScaleBiasClamp();
		bMapRange = ScaleBiasClamp.bMapRange;
		bClampResult = ScaleBiasClamp.bClampResult;
		bInterpResult = ScaleBiasClamp.bInterpResult;
		InRange = ScaleBiasClamp.InRange;
		OutRange = ScaleBiasClamp.OutRange;
		Scale = ScaleBiasClamp.Scale;
		Bias = ScaleBiasClamp.Bias;
		ClampMin = ScaleBiasClamp.ClampMin;
		ClampMax = ScaleBiasClamp.ClampMax;
		InterpSpeedIncreasing = ScaleBiasClamp.InterpSpeedIncreasing;
		InterpSpeedDecreasing = ScaleBiasClamp.InterpSpeedDecreasing;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	virtual FString ProcessPinLabelForInjection(const FString& InLabel) const override;

	UPROPERTY(meta=(Input))
	FVector Value;

	UPROPERTY(meta = (Input))
	float Scale;

	UPROPERTY(meta = (Input))
	float Bias;

	UPROPERTY(meta = (Input, Constant))
	bool bMapRange;

	UPROPERTY(meta = (Input, EditCondition = "bMapRange"))
	FInputRange InRange;

	UPROPERTY(meta = (Input, EditCondition = "bMapRange"))
	FInputRange OutRange;

	UPROPERTY(meta = (Input, Constant))
	bool bClampResult;

	UPROPERTY(meta = (Input, EditCondition = "bClampResult"))
	float ClampMin;

	UPROPERTY(meta = (Input, EditCondition = "bClampResult"))
	float ClampMax;

	UPROPERTY(meta = (Input, Constant))
	bool bInterpResult;

	UPROPERTY(meta = (Input, EditCondition = "bInterpResult"))
	float InterpSpeedIncreasing;

	UPROPERTY(meta = (Input, EditCondition = "bInterpResult"))
	float InterpSpeedDecreasing;

	UPROPERTY(meta=(Output))
	FVector Result;

	UPROPERTY()
	FInputScaleBiasClamp ScaleBiasClamp;
};

/**
 * Takes in a vector value and outputs an accumulated value with a customized scale and clamp
 */
USTRUCT(meta=(DisplayName="Alpha Interpolate", Keywords="Alpha,Lerp,LinearInterpolate", Category = "Simulation|Time", TemplateName="AlphaInterp", MenuDescSuffix = "(Quat)"))
struct CONTROLRIG_API FRigUnit_AlphaInterpQuat : public FRigUnit_SimBase
{
	GENERATED_BODY()
	
	FRigUnit_AlphaInterpQuat()
	{
		Value = Result = FQuat::Identity;
		ScaleBiasClamp = FInputScaleBiasClamp();
		bMapRange = ScaleBiasClamp.bMapRange;
		bClampResult = ScaleBiasClamp.bClampResult;
		bInterpResult = ScaleBiasClamp.bInterpResult;
		InRange = ScaleBiasClamp.InRange;
		OutRange = ScaleBiasClamp.OutRange;
		Scale = ScaleBiasClamp.Scale;
		Bias = ScaleBiasClamp.Bias;
		ClampMin = ScaleBiasClamp.ClampMin;
		ClampMax = ScaleBiasClamp.ClampMax;
		InterpSpeedIncreasing = ScaleBiasClamp.InterpSpeedIncreasing;
		InterpSpeedDecreasing = ScaleBiasClamp.InterpSpeedDecreasing;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	virtual FString ProcessPinLabelForInjection(const FString& InLabel) const override;

	UPROPERTY(meta=(Input))
	FQuat Value;

	UPROPERTY(meta = (Input))
	float Scale;

	UPROPERTY(meta = (Input))
	float Bias;

	UPROPERTY(meta = (Input, Constant))
	bool bMapRange;

	UPROPERTY(meta = (Input, EditCondition = "bMapRange"))
	FInputRange InRange;

	UPROPERTY(meta = (Input, EditCondition = "bMapRange"))
	FInputRange OutRange;

	UPROPERTY(meta = (Input, Constant))
	bool bClampResult;

	UPROPERTY(meta = (Input, EditCondition = "bClampResult"))
	float ClampMin;

	UPROPERTY(meta = (Input, EditCondition = "bClampResult"))
	float ClampMax;

	UPROPERTY(meta = (Input, Constant))
	bool bInterpResult;

	UPROPERTY(meta = (Input, EditCondition = "bInterpResult"))
	float InterpSpeedIncreasing;

	UPROPERTY(meta = (Input, EditCondition = "bInterpResult"))
	float InterpSpeedDecreasing;

	UPROPERTY(meta=(Output))
	FQuat Result;

	UPROPERTY()
	FInputScaleBiasClamp ScaleBiasClamp;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunction_MathBase.h"
#include "RigVMFunction_Noise.generated.h"

/**
 * Generates a float through a noise fluctuation process between a min and a max through speed
 */
USTRUCT(meta=(DisplayName="Noise (Float)", Category="Math|Noise", TemplateName="Noise"))
struct RIGVM_API FRigVMFunction_NoiseFloat : public FRigVMFunction_MathBase
{
	GENERATED_BODY()

	FRigVMFunction_NoiseFloat()
	{
		Value = Minimum = Result = Time = 0.f;
		Speed = 0.1f;
		Frequency = Maximum = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	float Value;

	UPROPERTY(meta = (Input))
	float Speed;

	UPROPERTY(meta = (Input))
	float Frequency;

	UPROPERTY(meta = (Input))
	float Minimum;

	UPROPERTY(meta = (Input))
	float Maximum;

	UPROPERTY(meta = (Output))
	float Result;

	UPROPERTY()
	float Time;
};

/**
 * Generates a float through a noise fluctuation process between a min and a max through speed
 */
USTRUCT(meta=(DisplayName="Noise (Double)", Category="Math|Noise", TemplateName="Noise"))
struct RIGVM_API FRigVMFunction_NoiseDouble : public FRigVMFunction_MathBase
{
	GENERATED_BODY()

	FRigVMFunction_NoiseDouble()
	{
		Value = Minimum = Result = Time = 0.0;
		Speed = 0.1;
		Frequency = Maximum = 1.0;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	double Value;

	UPROPERTY(meta = (Input))
	double Speed;

	UPROPERTY(meta = (Input))
	double Frequency;

	UPROPERTY(meta = (Input))
	double Minimum;

	UPROPERTY(meta = (Input))
	double Maximum;

	UPROPERTY(meta = (Output))
	double Result;

	UPROPERTY()
	double Time;
};

/**
 * Generates a vector through a noise fluctuation process between a min and a max through speed
 */
USTRUCT(meta = (DisplayName = "Noise (Vector)", Category = "Math|Noise", TemplateName="Noise", Deprecated = "5.0.0"))
struct RIGVM_API FRigVMFunction_NoiseVector : public FRigVMFunction_MathBase
{
	GENERATED_BODY()

	FRigVMFunction_NoiseVector()
	{
		Position = Result = Time = FVector::ZeroVector;
		Frequency = FVector::OneVector;
		Speed = FVector(0.1f, 0.1f, 0.1f);
		Minimum = 0.f;
		Maximum = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FVector Position;

	UPROPERTY(meta = (Input))
	FVector Speed;

	UPROPERTY(meta = (Input))
	FVector Frequency;

	UPROPERTY(meta = (Input))
	float Minimum;

	UPROPERTY(meta = (Input))
	float Maximum;

	UPROPERTY(meta = (Output))
	FVector Result;

	UPROPERTY()
	FVector Time;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Generates a vector through a noise fluctuation process between a min and a max through speed
 */
USTRUCT(meta = (DisplayName = "Noise (Vector)", Category = "Math|Noise", TemplateName="Noise"))
struct RIGVM_API FRigVMFunction_NoiseVector2 : public FRigVMFunction_MathBase
{
	GENERATED_BODY()

	FRigVMFunction_NoiseVector2()
	{
		Value = Result = Time = FVector::ZeroVector;
		Frequency = FVector::OneVector;
		Speed = FVector(0.1f, 0.1f, 0.1f);
		Minimum = 0.0;
		Maximum = 1.0;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FVector Value;

	UPROPERTY(meta = (Input))
	FVector Speed;

	UPROPERTY(meta = (Input))
	FVector Frequency;

	UPROPERTY(meta = (Input))
	double Minimum;

	UPROPERTY(meta = (Input))
	double Maximum;

	UPROPERTY(meta = (Output))
	FVector Result;

	UPROPERTY()
	FVector Time;
};
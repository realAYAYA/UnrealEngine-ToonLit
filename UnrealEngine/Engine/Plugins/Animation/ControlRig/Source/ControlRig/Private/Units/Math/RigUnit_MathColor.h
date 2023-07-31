// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_MathBase.h"
#include "Math/ControlRigMathLibrary.h"
#include "RigUnit_MathColor.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Color", MenuDescSuffix="(Color)"))
struct CONTROLRIG_API FRigUnit_MathColorBase : public FRigUnit_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathColorBinaryOp : public FRigUnit_MathColorBase
{
	GENERATED_BODY()

	FRigUnit_MathColorBinaryOp()
	{
		A = B = Result = FLinearColor::Black;
	}

	UPROPERTY(meta=(Input))
	FLinearColor A;

	UPROPERTY(meta=(Input))
	FLinearColor B;

	UPROPERTY(meta=(Output))
	FLinearColor Result;
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathColorBinaryAggregateOp : public FRigUnit_MathColorBase
{
	GENERATED_BODY()

	FRigUnit_MathColorBinaryAggregateOp()
	{
		A = B = Result = FLinearColor::Black;
	}

	UPROPERTY(meta=(Input, Aggregate))
	FLinearColor A;

	UPROPERTY(meta=(Input, Aggregate))
	FLinearColor B;

	UPROPERTY(meta=(Output, Aggregate))
	FLinearColor Result;
};

/**
 * Makes a vector from a single float
 */
USTRUCT(meta=(DisplayName="From Float", TemplateName="Cast", Keywords="Make,Construct"))
struct CONTROLRIG_API FRigUnit_MathColorFromFloat : public FRigUnit_MathColorBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	FRigUnit_MathColorFromFloat()
	{
		Value = 0.f;
		Result = FLinearColor::Black;
	}

	UPROPERTY(meta=(Input))
	float Value;

	UPROPERTY(meta=(Output))
	FLinearColor Result;
};

/**
 * Returns the sum of the two values
 */
USTRUCT(meta=(DisplayName="Add", TemplateName="Add", Keywords="Sum,+"))
struct CONTROLRIG_API FRigUnit_MathColorAdd : public FRigUnit_MathColorBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the difference of the two values
 */
USTRUCT(meta=(DisplayName="Subtract", TemplateName="Subtract", Keywords="-"))
struct CONTROLRIG_API FRigUnit_MathColorSub : public FRigUnit_MathColorBinaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the product of the two values
 */
USTRUCT(meta=(DisplayName="Multiply", TemplateName="Multiply", Keywords="Product,*"))
struct CONTROLRIG_API FRigUnit_MathColorMul : public FRigUnit_MathColorBinaryAggregateOp
{
	GENERATED_BODY()

	FRigUnit_MathColorMul()
	{
		A = B = FLinearColor::White;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Linearly interpolates between A and B using the ratio T
 */
USTRUCT(meta=(DisplayName="Interpolate", TemplateName="Interpolate", Keywords="Lerp,Mix,Blend"))
struct CONTROLRIG_API FRigUnit_MathColorLerp : public FRigUnit_MathColorBase
{
	GENERATED_BODY()

	FRigUnit_MathColorLerp()
	{
		A = Result = FLinearColor::Black;
		B = FLinearColor::White;
		T = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FLinearColor A;

	UPROPERTY(meta=(Input))
	FLinearColor B;

	UPROPERTY(meta=(Input))
	float T;

	UPROPERTY(meta=(Output))
	FLinearColor Result;
};


// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunction_MathBase.h"
#include "RigVMFunction_MathColor.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Color", MenuDescSuffix="(Color)"))
struct RIGVM_API FRigVMFunction_MathColorBase : public FRigVMFunction_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathColorBinaryOp : public FRigVMFunction_MathColorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathColorBinaryOp()
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
struct RIGVM_API FRigVMFunction_MathColorBinaryAggregateOp : public FRigVMFunction_MathColorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathColorBinaryAggregateOp()
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
 * Makes a color from its components
 */
USTRUCT(meta=(DisplayName="Make Color", Keywords="Make,Construct,Constant"))
struct RIGVM_API FRigVMFunction_MathColorMake : public FRigVMFunction_MathColorBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathColorMake()
	{
		R = G = B = 0.f;
		A = 1.f;
		Result = FLinearColor::Black;
	}

	UPROPERTY(meta=(Input))
	float R;

	UPROPERTY(meta=(Input))
	float G;

	UPROPERTY(meta=(Input))
	float B;

	UPROPERTY(meta=(Input))
	float A;

	UPROPERTY(meta=(Output))
	FLinearColor Result;
};

/**
 * Makes a vector from a single float
 */
USTRUCT(meta=(DisplayName="From Float", TemplateName="Cast", ExecuteContext="FRigVMExecuteContext", Keywords="Make,Construct"))
struct RIGVM_API FRigVMFunction_MathColorFromFloat : public FRigVMFunction_MathColorBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathColorFromFloat()
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
 * Makes a vector from a single double
 */
USTRUCT(meta=(DisplayName="From Double", TemplateName="Cast", ExecuteContext="FRigVMExecuteContext", Keywords="Make,Construct"))
struct RIGVM_API FRigVMFunction_MathColorFromDouble : public FRigVMFunction_MathColorBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathColorFromDouble()
	{
		Value = 0.0;
		Result = FLinearColor::Black;
	}

	UPROPERTY(meta=(Input))
	double Value;

	UPROPERTY(meta=(Output))
	FLinearColor Result;
};

/**
 * Returns the sum of the two values
 */
USTRUCT(meta=(DisplayName="Add", TemplateName="Add", Keywords="Sum,+"))
struct RIGVM_API FRigVMFunction_MathColorAdd : public FRigVMFunction_MathColorBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the difference of the two values
 */
USTRUCT(meta=(DisplayName="Subtract", TemplateName="Subtract", Keywords="-"))
struct RIGVM_API FRigVMFunction_MathColorSub : public FRigVMFunction_MathColorBinaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the product of the two values
 */
USTRUCT(meta=(DisplayName="Multiply", TemplateName="Multiply", Keywords="Product,*"))
struct RIGVM_API FRigVMFunction_MathColorMul : public FRigVMFunction_MathColorBinaryAggregateOp
{
	GENERATED_BODY()

	FRigVMFunction_MathColorMul()
	{
		A = B = FLinearColor::White;
	}

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Linearly interpolates between A and B using the ratio T
 */
USTRUCT(meta=(DisplayName="Interpolate", TemplateName="Interpolate", Keywords="Lerp,Mix,Blend"))
struct RIGVM_API FRigVMFunction_MathColorLerp : public FRigVMFunction_MathColorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathColorLerp()
	{
		A = Result = FLinearColor::Black;
		B = FLinearColor::White;
		T = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FLinearColor A;

	UPROPERTY(meta=(Input))
	FLinearColor B;

	UPROPERTY(meta=(Input))
	float T;

	UPROPERTY(meta=(Output))
	FLinearColor Result;
};


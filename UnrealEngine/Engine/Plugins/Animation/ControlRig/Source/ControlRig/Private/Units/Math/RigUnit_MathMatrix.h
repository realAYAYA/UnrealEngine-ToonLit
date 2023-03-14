// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EulerTransform.h"
#include "RigUnit_MathBase.h"
#include "Math/ControlRigMathLibrary.h"
#include "RigUnit_MathMatrix.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Matrix", MenuDescSuffix="(Matrix)"))
struct CONTROLRIG_API FRigUnit_MathMatrixBase : public FRigUnit_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathMatrixUnaryOp : public FRigUnit_MathMatrixBase
{
	GENERATED_BODY()

	FRigUnit_MathMatrixUnaryOp()
	{
		Value = Result = FMatrix::Identity;
	}

	UPROPERTY(meta=(Input))
	FMatrix Value;

	UPROPERTY(meta=(Output))
	FMatrix Result;
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathMatrixBinaryOp : public FRigUnit_MathMatrixBase
{
	GENERATED_BODY()

	FRigUnit_MathMatrixBinaryOp()
	{
		A = B = Result = FMatrix::Identity;
	}

	UPROPERTY(meta=(Input))
	FMatrix A;

	UPROPERTY(meta=(Input))
	FMatrix B;

	UPROPERTY(meta=(Output))
	FMatrix Result;
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathMatrixBinaryAggregateOp : public FRigUnit_MathMatrixBase
{
	GENERATED_BODY()

	FRigUnit_MathMatrixBinaryAggregateOp()
	{
		A = B = Result = FMatrix::Identity;
	}

	UPROPERTY(meta=(Input, Aggregate))
	FMatrix A;

	UPROPERTY(meta=(Input, Aggregate))
	FMatrix B;

	UPROPERTY(meta=(Output, Aggregate))
	FMatrix Result;
};

/**
* Makes a transform from a matrix
*/
USTRUCT(meta=(DisplayName="To Transform", TemplateName="Cast", Keywords="Make,Construct"))
struct CONTROLRIG_API FRigUnit_MathMatrixToTransform : public FRigUnit_MathMatrixBase
{
	GENERATED_BODY()

	FRigUnit_MathMatrixToTransform()
	{
		Value = FMatrix::Identity;
		Result = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FMatrix Value;

	UPROPERTY(meta=(Output))
	FTransform Result;
};

/**
 * Makes a matrix from a transform
 */
USTRUCT(meta=(DisplayName="From Transform", Keywords="Make,Construct", Deprecated="5.0.1"))
struct CONTROLRIG_API FRigUnit_MathMatrixFromTransform : public FRigUnit_MathMatrixBase
{
	GENERATED_BODY()

	FRigUnit_MathMatrixFromTransform()
	{
		Transform = FTransform::Identity;
		Result = FMatrix::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FTransform Transform;

	UPROPERTY(meta=(Output))
	FMatrix Result;
	
	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Makes a matrix from a transform
 */
USTRUCT(meta=(DisplayName="From Transform", TemplateName="Cast", Keywords="Make,Construct"))
struct CONTROLRIG_API FRigUnit_MathMatrixFromTransformV2 : public FRigUnit_MathMatrixBase
{
	GENERATED_BODY()

	FRigUnit_MathMatrixFromTransformV2()
	{
		Value = FTransform::Identity;
		Result = FMatrix::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FTransform Value;

	UPROPERTY(meta=(Output))
	FMatrix Result;
};

/**
 * Converts the matrix to its vectors
 */
USTRUCT(meta=(DisplayName="To Vectors", Keywords="Make,Construct"))
struct CONTROLRIG_API FRigUnit_MathMatrixToVectors : public FRigUnit_MathMatrixBase
{
	GENERATED_BODY()

	FRigUnit_MathMatrixToVectors()
	{
		Value = FMatrix::Identity;
		Origin = FVector::ZeroVector;
		X = FVector::XAxisVector;
		Y = FVector::YAxisVector;
		Z = FVector::ZAxisVector;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FMatrix Value;

	UPROPERTY(meta=(Output))
	FVector Origin;

	UPROPERTY(meta=(Output))
	FVector X;

	UPROPERTY(meta=(Output))
	FVector Y;

	UPROPERTY(meta=(Output))
	FVector Z;
};

/**
* Makes a matrix from its vectors
*/
USTRUCT(meta=(DisplayName="From Vectors", Keywords="Make,Construct"))
struct CONTROLRIG_API FRigUnit_MathMatrixFromVectors : public FRigUnit_MathMatrixBase
{
	GENERATED_BODY()

	FRigUnit_MathMatrixFromVectors()
	{
		Origin = FVector::ZeroVector;
		X = FVector::XAxisVector;
		Y = FVector::YAxisVector;
		Z = FVector::ZAxisVector;
		Result = FMatrix::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FVector Origin;

	UPROPERTY(meta=(Input))
	FVector X;

	UPROPERTY(meta=(Input))
	FVector Y;

	UPROPERTY(meta=(Input))
	FVector Z;

	UPROPERTY(meta=(Output))
	FMatrix Result;
};

/**
 * Returns the product of the two values
 */
USTRUCT(meta=(DisplayName="Multiply", TemplateName="Multiply", Keywords="Product,*,Global"))
struct CONTROLRIG_API FRigUnit_MathMatrixMul : public FRigUnit_MathMatrixBinaryAggregateOp
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the inverse value
 */
USTRUCT(meta=(DisplayName="Inverse", TemplateName="Inverse"))
struct CONTROLRIG_API FRigUnit_MathMatrixInverse : public FRigUnit_MathMatrixUnaryOp
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EulerTransform.h"
#include "RigVMFunction_MathBase.h"
#include "RigVMFunction_MathMatrix.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Matrix", MenuDescSuffix="(Matrix)"))
struct RIGVM_API FRigVMFunction_MathMatrixBase : public FRigVMFunction_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathMatrixUnaryOp : public FRigVMFunction_MathMatrixBase
{
	GENERATED_BODY()

	FRigVMFunction_MathMatrixUnaryOp()
	{
		Value = Result = FMatrix::Identity;
	}

	UPROPERTY(meta=(Input))
	FMatrix Value;

	UPROPERTY(meta=(Output))
	FMatrix Result;
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathMatrixBinaryOp : public FRigVMFunction_MathMatrixBase
{
	GENERATED_BODY()

	FRigVMFunction_MathMatrixBinaryOp()
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
struct RIGVM_API FRigVMFunction_MathMatrixBinaryAggregateOp : public FRigVMFunction_MathMatrixBase
{
	GENERATED_BODY()

	FRigVMFunction_MathMatrixBinaryAggregateOp()
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
USTRUCT(meta=(DisplayName="To Transform", TemplateName="Cast", ExecuteContext="FRigVMExecuteContext", Keywords="Make,Construct"))
struct RIGVM_API FRigVMFunction_MathMatrixToTransform : public FRigVMFunction_MathMatrixBase
{
	GENERATED_BODY()

	FRigVMFunction_MathMatrixToTransform()
	{
		Value = FMatrix::Identity;
		Result = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FMatrix Value;

	UPROPERTY(meta=(Output))
	FTransform Result;
};

/**
 * Makes a matrix from a transform
 */
USTRUCT(meta=(DisplayName="From Transform", Keywords="Make,Construct", Deprecated="5.0.1"))
struct RIGVM_API FRigVMFunction_MathMatrixFromTransform : public FRigVMFunction_MathMatrixBase
{
	GENERATED_BODY()

	FRigVMFunction_MathMatrixFromTransform()
	{
		Transform = FTransform::Identity;
		Result = FMatrix::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
USTRUCT(meta=(DisplayName="From Transform", TemplateName="Cast", ExecuteContext="FRigVMExecuteContext", Keywords="Make,Construct"))
struct RIGVM_API FRigVMFunction_MathMatrixFromTransformV2 : public FRigVMFunction_MathMatrixBase
{
	GENERATED_BODY()

	FRigVMFunction_MathMatrixFromTransformV2()
	{
		Value = FTransform::Identity;
		Result = FMatrix::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FTransform Value;

	UPROPERTY(meta=(Output))
	FMatrix Result;
};

/**
 * Converts the matrix to its vectors
 */
USTRUCT(meta=(DisplayName="To Vectors", Keywords="Make,Construct"))
struct RIGVM_API FRigVMFunction_MathMatrixToVectors : public FRigVMFunction_MathMatrixBase
{
	GENERATED_BODY()

	FRigVMFunction_MathMatrixToVectors()
	{
		Value = FMatrix::Identity;
		Origin = FVector::ZeroVector;
		X = FVector::XAxisVector;
		Y = FVector::YAxisVector;
		Z = FVector::ZAxisVector;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathMatrixFromVectors : public FRigVMFunction_MathMatrixBase
{
	GENERATED_BODY()

	FRigVMFunction_MathMatrixFromVectors()
	{
		Origin = FVector::ZeroVector;
		X = FVector::XAxisVector;
		Y = FVector::YAxisVector;
		Z = FVector::ZAxisVector;
		Result = FMatrix::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathMatrixMul : public FRigVMFunction_MathMatrixBinaryAggregateOp
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the inverse value
 */
USTRUCT(meta=(DisplayName="Inverse", TemplateName="Inverse"))
struct RIGVM_API FRigVMFunction_MathMatrixInverse : public FRigVMFunction_MathMatrixUnaryOp
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;
};

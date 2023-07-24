// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "MathLibrary.h"
#include "Math/ControlRigMathLibrary.h"
#include "RigVMFunctions/Math/RigVMFunction_MathVector.h"
#include "RigUnit_Vector.generated.h"

/** Two args and a result of Vector type */
USTRUCT(meta=(Abstract, NodeColor = "0.1 0.7 0.1"))
struct CONTROLRIG_API FRigUnit_BinaryVectorOp : public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta=(Input))
	FVector Argument0 = FVector(0.f);

	UPROPERTY(meta=(Input))
	FVector Argument1 = FVector(0.f);

	UPROPERTY(meta=(Output))
	FVector Result = FVector(0.f);
};

USTRUCT(meta=(DisplayName="Multiply(Vector)", Category="Math|Vector", Deprecated="4.23.0"))
struct CONTROLRIG_API FRigUnit_Multiply_VectorVector : public FRigUnit_BinaryVectorOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

USTRUCT(meta=(DisplayName="Add(Vector)", Category="Math|Vector", Deprecated="4.23.0"))
struct CONTROLRIG_API FRigUnit_Add_VectorVector : public FRigUnit_BinaryVectorOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

USTRUCT(meta=(DisplayName="Subtract(Vector)", Category="Math|Vector", Deprecated="4.23.0"))
struct CONTROLRIG_API FRigUnit_Subtract_VectorVector : public FRigUnit_BinaryVectorOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

USTRUCT(meta=(DisplayName="Divide(Vector)", Category="Math|Vector", Deprecated="4.23.0"))
struct CONTROLRIG_API FRigUnit_Divide_VectorVector : public FRigUnit_BinaryVectorOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

USTRUCT(meta = (DisplayName = "Distance", Category = "Math|Vector", Deprecated="4.23.0", NodeColor = "0.1 0.7 0.1"))
struct CONTROLRIG_API FRigUnit_Distance_VectorVector : public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta=(Input))
	FVector Argument0 = FVector(0.f);

	UPROPERTY(meta=(Input))
	FVector Argument1 = FVector(0.f);

	UPROPERTY(meta=(Output))
	float Result = 0.f;

	RIGVM_METHOD()
	virtual void Execute() override;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns the 4 point bezier interpolation
 */
USTRUCT(meta=(DisplayName="Bezier Four Point", Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_MathVectorBezierFourPoint : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorBezierFourPoint()
	{
		Bezier = FRigVMFourPointBezier();
		T = 0.f;
		Result = Tangent = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FRigVMFourPointBezier Bezier;

	UPROPERTY(meta=(Input))
	float T;

	UPROPERTY(meta=(Output))
	FVector Result;

	UPROPERTY(meta=(Output))
	FVector Tangent;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Creates a bezier four point
 */
USTRUCT(meta = (DisplayName = "Make Bezier Four Point", Constant, Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_MathVectorMakeBezierFourPoint : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorMakeBezierFourPoint()
	{
		Bezier = FRigVMFourPointBezier();
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input, Output))
	FRigVMFourPointBezier Bezier;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

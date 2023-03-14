// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "MathLibrary.h"
#include "RigUnit_Transform.generated.h"

/** Two args and a result of Transform type */
USTRUCT(meta=(Abstract, NodeColor = "0.1 0.7 0.1"))
struct CONTROLRIG_API FRigUnit_BinaryTransformOp : public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta=(Input))
	FTransform Argument0;

	UPROPERTY(meta=(Input))
	FTransform Argument1;

	UPROPERTY(meta=(Output))
	FTransform Result;
};

USTRUCT(meta=(DisplayName="Multiply(Transform)", Category="Math|Transform", Deprecated="4.23.0"))
struct CONTROLRIG_API FRigUnit_MultiplyTransform : public FRigUnit_BinaryTransformOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

USTRUCT(meta = (DisplayName = "GetRelativeTransform", Category = "Math|Transform", Deprecated="4.23.0"))
struct CONTROLRIG_API FRigUnit_GetRelativeTransform : public FRigUnit_BinaryTransformOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};


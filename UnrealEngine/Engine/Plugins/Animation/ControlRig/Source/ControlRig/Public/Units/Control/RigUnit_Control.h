// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "Constraint.h"
#include "EulerTransform.h"
#include "RigUnit_Control.generated.h"

/** A control unit used to drive a transform from an external source */
USTRUCT(meta=(DisplayName="Control", Category="Controls", ShowVariableNameInTitle, Deprecated = "4.24.0"))
struct CONTROLRIG_API FRigUnit_Control : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_Control()
		: Transform(FEulerTransform::Identity)
		, Base(FTransform::Identity)
		, Result(FTransform::Identity)
	{
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/** Combine Transform and Base to make the resultant transform */
	FTransform GetResultantTransform() const;
	static FTransform StaticGetResultantTransform(const FEulerTransform& InTransform, const FTransform& InBase, const FTransformFilter& InFilter);

	/** Combine Transform and Base to make the resultant transform (as a matrix) */
	FMatrix GetResultantMatrix() const;
	static FMatrix StaticGetResultantMatrix(const FEulerTransform& InTransform, const FTransform& InBase, const FTransformFilter& InFilter);

	/** Set the transform using a resultant transform (already incorporating Base) */
	void SetResultantTransform(const FTransform& InResultantTransform);
	static void StaticSetResultantTransform(const FTransform& InResultantTransform, const FTransform& InBase, FEulerTransform& OutTransform);

	/** Set the transform using a resultant matrix (already incorporating Base) */
	void SetResultantMatrix(const FMatrix& InResultantMatrix);
	static void StaticSetResultantMatrix(const FMatrix& InResultantMatrix, const FTransform& InBase, FEulerTransform& OutTransform);

	/** Get the local transform (i.e. without base) with filter applied */
	FEulerTransform GetFilteredTransform() const;
	static FEulerTransform StaticGetFilteredTransform(const FEulerTransform& InTransform, const FTransformFilter& InFilter);

	/** The transform of this control */
	UPROPERTY(EditAnywhere, Category="Control", Interp, meta = (AnimationInput))
	FEulerTransform Transform;

	/** The base that transform is relative to */
	UPROPERTY(meta=(Input))
	FTransform Base;

	/** The initial transform that The Transform is initialized to. */
	UPROPERTY(meta = (Input))
	FTransform InitTransform;

	/** The resultant transform of this unit (Base * Filter(Transform)) */
	UPROPERTY(meta=(Output))
	FTransform Result;

	/** The filter determines what axes can be manipulated by the in-viewport widgets */
	UPROPERTY(meta=(Input))
 	FTransformFilter Filter;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};
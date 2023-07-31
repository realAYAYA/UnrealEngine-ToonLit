// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_DebugBase.h"
#include "RigUnit_DebugLine.generated.h"

USTRUCT(meta=(DisplayName="Draw Line", Deprecated = "4.25"))
struct CONTROLRIG_API FRigUnit_DebugLine : public FRigUnit_DebugBaseMutable
{
	GENERATED_BODY()

	FRigUnit_DebugLine()
	{
		A = B = FVector::ZeroVector;
		Color = FLinearColor::Red;
		Thickness = 0.f;
		WorldOffset = FTransform::Identity;
		bEnabled = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FVector A;

	UPROPERTY(meta = (Input))
	FVector B;

	UPROPERTY(meta = (Input))
	FLinearColor Color;

	UPROPERTY(meta = (Input))
	float Thickness;

	UPROPERTY(meta = (Input))
	FName Space;

	UPROPERTY(meta = (Input))
	FTransform WorldOffset;

	UPROPERTY(meta = (Input, Constant))
	bool bEnabled;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Draws a line in the viewport given a start and end vector
 */
USTRUCT(meta=(DisplayName="Draw Line"))
struct CONTROLRIG_API FRigUnit_DebugLineItemSpace : public FRigUnit_DebugBaseMutable
{
	GENERATED_BODY()

	FRigUnit_DebugLineItemSpace()
	{
		A = B = FVector::ZeroVector;
		Color = FLinearColor::Red;
		Thickness = 0.f;
		Space = FRigElementKey(NAME_None, ERigElementType::Bone);
		WorldOffset = FTransform::Identity;
		bEnabled = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FVector A;

	UPROPERTY(meta = (Input))
	FVector B;

	UPROPERTY(meta = (Input))
	FLinearColor Color;

	UPROPERTY(meta = (Input))
	float Thickness;

	UPROPERTY(meta = (Input))
	FRigElementKey Space;

	UPROPERTY(meta = (Input))
	FTransform WorldOffset;

	UPROPERTY(meta = (Input))
	bool bEnabled;
};
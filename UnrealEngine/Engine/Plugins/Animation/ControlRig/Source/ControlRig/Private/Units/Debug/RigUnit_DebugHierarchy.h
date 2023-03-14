// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_DebugBase.h"
#include "Drawing/ControlRigDrawInterface.h"
#include "RigUnit_DebugHierarchy.generated.h"

/**
 * Draws vectors on each bone in the viewport across the entire hierarchy
 */
USTRUCT(meta=(DisplayName="Draw Hierarchy"))
struct CONTROLRIG_API FRigUnit_DebugHierarchy : public FRigUnit_DebugBaseMutable
{
	GENERATED_BODY()

	FRigUnit_DebugHierarchy()
	{
		Scale = 10.f;
		Color = FLinearColor::White;
		Thickness = 0.f;
		WorldOffset = FTransform::Identity;
		bEnabled = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	float Scale;

	UPROPERTY(meta = (Input))
	FLinearColor Color;

	UPROPERTY(meta = (Input))
	float Thickness;

	UPROPERTY(meta = (Input))
	FTransform WorldOffset;

	UPROPERTY(meta = (Input))
	bool bEnabled;
};

/**
* Draws vectors on each bone in the viewport across the entire pose
*/
USTRUCT(meta=(DisplayName="Draw Pose Cache"))
struct CONTROLRIG_API FRigUnit_DebugPose : public FRigUnit_DebugBaseMutable
{
	GENERATED_BODY()

	FRigUnit_DebugPose()
	{
		Scale = 10.f;
		Color = FLinearColor::White;
		Thickness = 0.f;
		WorldOffset = FTransform::Identity;
		bEnabled = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigPose Pose;

	UPROPERTY(meta = (Input))
	float Scale;

	UPROPERTY(meta = (Input))
	FLinearColor Color;

	UPROPERTY(meta = (Input))
	float Thickness;

	UPROPERTY(meta = (Input))
	FTransform WorldOffset;

	UPROPERTY(meta = (Input))
	bool bEnabled;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunctions/Debug/RigVMFunction_DebugBase.h"
#include "Rigs/RigHierarchy.h"
#include "Units/RigUnitContext.h"
#include "RigUnit_DebugHierarchy.generated.h"

UENUM()
namespace EControlRigDrawHierarchyMode
{
	enum Type : int
	{
		/** Draw as axes */
		Axes,

		/** MAX - invalid */
		Max UMETA(Hidden),
	};
}

/**
 * Draws vectors on each bone in the viewport across the entire hierarchy
 */
USTRUCT(meta=(DisplayName="Draw Hierarchy"))
struct CONTROLRIG_API FRigUnit_DebugHierarchy : public FRigVMFunction_DebugBase
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
	virtual void Execute() override;

	UPROPERTY(DisplayName = "Execute", Transient, meta = (Input, Output))
	FControlRigExecuteContext ExecuteContext;

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

	static void DrawHierarchy(const FRigVMExecuteContext& InContext, const FTransform& WorldOffset, URigHierarchy* Hierarchy, EControlRigDrawHierarchyMode::Type Mode, float Scale, const FLinearColor& Color, float Thickness, const FRigPose* InPose);
};

/**
* Draws vectors on each bone in the viewport across the entire pose
*/
USTRUCT(meta=(DisplayName="Draw Pose Cache"))
struct CONTROLRIG_API FRigUnit_DebugPose : public FRigVMFunction_DebugBase
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
	virtual void Execute() override;

	UPROPERTY(DisplayName = "Execute", Transient, meta = (Input, Output))
	FControlRigExecuteContext ExecuteContext;

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
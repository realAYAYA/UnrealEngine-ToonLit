// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunction_DebugBase.h"
#include "RigVMFunction_DebugTransform.generated.h"

UENUM(meta = (RigVMTypeAllowed))
enum class ERigUnitDebugTransformMode : uint8
{
	/** Draw as point */
	Point,

	/** Draw as axes */
	Axes,

	/** Draw as box */
	Box,

	/** MAX - invalid */
	Max UMETA(Hidden),
};

/**
 * Given a transform, will draw a point, axis, or a box in the viewport
 */
USTRUCT(meta=(DisplayName="Draw Transform"))
struct RIGVM_API FRigVMFunction_DebugTransformMutableNoSpace : public FRigVMFunction_DebugBaseMutable
{
	GENERATED_BODY()

	FRigVMFunction_DebugTransformMutableNoSpace()
	{
		Transform = WorldOffset = FTransform::Identity;
		Mode = ERigUnitDebugTransformMode::Axes;
		Color = FLinearColor::Red;
		Thickness = 0.f;
		Scale = 10.f;
		bEnabled = true;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FTransform Transform;

	UPROPERTY(meta = (Input))
	ERigUnitDebugTransformMode Mode;

	UPROPERTY(meta = (Input))
	FLinearColor Color;

	UPROPERTY(meta = (Input))
	float Thickness;

	UPROPERTY(meta = (Input))
	float Scale;

	UPROPERTY(meta = (Input))
	FTransform WorldOffset;

	UPROPERTY(meta = (Input))
	bool bEnabled;
};

USTRUCT()
struct RIGVM_API FRigVMFunction_DebugTransformArrayMutable_WorkData
{
	GENERATED_BODY()
		
	UPROPERTY()
	TArray<FTransform> DrawTransforms;
};

/**
* Given a transform array, will draw a point, axis, or a box in the viewport
*/
USTRUCT(meta=(DisplayName="Draw Transform Array"))
struct RIGVM_API FRigVMFunction_DebugTransformArrayMutableNoSpace : public FRigVMFunction_DebugBaseMutable
{
	GENERATED_BODY()

	FRigVMFunction_DebugTransformArrayMutableNoSpace()
	{
		WorldOffset = FTransform::Identity;
		Mode = ERigUnitDebugTransformMode::Axes;
		Color = FLinearColor::White;
		Thickness = 0.f;
		Scale = 10.f;
		bEnabled = true;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	TArray<FTransform> Transforms;

	UPROPERTY(meta = (Input))
	TArray<int32> ParentIndices;

	UPROPERTY(meta = (Input))
	ERigUnitDebugTransformMode Mode;

	UPROPERTY(meta = (Input))
	FLinearColor Color;

	UPROPERTY(meta = (Input))
	float Thickness;

	UPROPERTY(meta = (Input))
	float Scale;

	UPROPERTY(meta = (Input))
	FTransform WorldOffset;

	UPROPERTY(meta = (Input))
	bool bEnabled;
};

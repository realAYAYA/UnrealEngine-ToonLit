// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_SetControlOffset.generated.h"

/**
 * SetControlOffset is used to perform a change in the hierarchy by setting a single control's transform.
 * This is typically only used during the Construction Event.
 */
USTRUCT(meta=(DisplayName="Set Control Offset", Category="Controls", DocumentationPolicy="Strict", Keywords = "SetControlOffset,Initial,InitialTransform,SetInitialTransform,SetInitialControlTransform", NodeColor="0, 0.364706, 1.0"))
struct CONTROLRIG_API FRigUnit_SetControlOffset : public FRigUnitMutable
{
	GENERATED_BODY()

	FRigUnit_SetControlOffset()
		: Control(NAME_None)
		, Space(EBoneGetterSetterMode::GlobalSpace)
	{
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The name of the Control to set the transform for.
	 */
	UPROPERTY(meta = (Input, CustomWidget = "ControlName" ))
	FName Control;

	/**
	 * The offset transform to set for the control
	 */
	UPROPERTY(meta = (Input, Output))
	FTransform Offset;

	/**
	 * Defines if the bone's transform should be set
	 * in local or global space.
	 */
	UPROPERTY(meta = (Input))
	EBoneGetterSetterMode Space;

	// user to internally cache the index of the bone
	UPROPERTY()
	FCachedRigElement CachedControlIndex;
};

/**
 * GetShapeTransform is used to retrieve single control's shape transform.
 * This is typically only used during the Construction Event.
 */
USTRUCT(meta=(DisplayName="Get Shape Transform", Category="Controls", DocumentationPolicy="Strict", Keywords = "GetControlShapeTransform,Gizmo,GizmoTransform,MeshTransform", NodeColor="0, 0.364706, 1.0"))
struct CONTROLRIG_API FRigUnit_GetShapeTransform : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_GetShapeTransform()
		: Control(NAME_None)
	{
		Transform = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The name of the Control to set the transform for.
	 */
	UPROPERTY(meta = (Input, CustomWidget = "ControlName" ))
	FName Control;

	/**
	 * The shape transform to set for the control
	 */
	UPROPERTY(meta = (Output))
	FTransform Transform;

	// user to internally cache the index of the bone
	UPROPERTY()
	FCachedRigElement CachedControlIndex;
};

/**
 * SetShapeTransform is used to perform a change in the hierarchy by setting a single control's shape transform.
 * This is typically only used during the Construction Event.
 */
USTRUCT(meta=(DisplayName="Set Shape Transform", Category="Controls", DocumentationPolicy="Strict", Keywords = "SetControlShapeTransform,Gizmo,GizmoTransform,MeshTransform", NodeColor="0, 0.364706, 1.0"))
struct CONTROLRIG_API FRigUnit_SetShapeTransform : public FRigUnitMutable
{
	GENERATED_BODY()

	FRigUnit_SetShapeTransform()
		: Control(NAME_None)
	{
		Transform = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The name of the Control to set the transform for.
	 */
	UPROPERTY(meta = (Input, CustomWidget = "ControlName" ))
	FName Control;

	/**
	 * The shape transform to set for the control
	 */
	UPROPERTY(meta = (Input))
	FTransform Transform;

	// user to internally cache the index of the bone
	UPROPERTY()
	FCachedRigElement CachedControlIndex;
};

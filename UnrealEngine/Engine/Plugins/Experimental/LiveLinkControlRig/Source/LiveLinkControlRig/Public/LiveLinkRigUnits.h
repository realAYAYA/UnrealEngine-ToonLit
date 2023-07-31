// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "ControlRigDefines.h"
#include "Units/RigUnit.h"
#include "Roles/LiveLinkAnimationBlueprintStructs.h"
#include "LiveLinkTypes.h"
#include "LiveLinkRigUnits.generated.h"

namespace LiveLinkControlRigUtilities
{
	ILiveLinkClient* TryGetLiveLinkClient();
}

USTRUCT(meta = (Abstract, NodeColor = "0.3 0.1 0.1"))
struct LIVELINKCONTROLRIG_API FRigUnit_LiveLinkBase : public FRigUnit
{
	GENERATED_BODY()
};

/**
 * Evaluate current Live Link Animation associated with supplied subject
 */
USTRUCT(meta = (DisplayName = "Evaluate Live Link Frame (Animation)", Category = "Live Link"))
struct LIVELINKCONTROLRIG_API FRigUnit_LiveLinkEvaluteFrameAnimation : public FRigUnit_LiveLinkBase
{
	GENERATED_BODY()

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FName SubjectName;

	UPROPERTY(meta = (Input))
	bool bDrawDebug = false;

	UPROPERTY(meta = (Input))
	FLinearColor DebugColor = FLinearColor::Red;

	UPROPERTY(meta = (Input))
	FTransform DebugDrawOffset;

	UPROPERTY(meta = (Output))
	FSubjectFrameHandle SubjectFrame;
};

/**
 * Get the transform value with supplied subject frame
 */
USTRUCT(meta = (DisplayName = "Get Transform By Name", Category = "Live Link"))
struct LIVELINKCONTROLRIG_API FRigUnit_LiveLinkGetTransformByName : public FRigUnit_LiveLinkBase
{
	GENERATED_BODY()

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FSubjectFrameHandle SubjectFrame;

	UPROPERTY(meta = (Input))
	FName TransformName;

	UPROPERTY(meta = (Input))
	EBoneGetterSetterMode Space = EBoneGetterSetterMode::LocalSpace;

	UPROPERTY(meta = (Output))
	FTransform Transform;
};

/**
 * Get the parameter value with supplied subject frame 
 */
USTRUCT(meta = (DisplayName = "Get Parameter Value By Name", Category = "Live Link"))
struct LIVELINKCONTROLRIG_API FRigUnit_LiveLinkGetParameterValueByName : public FRigUnit_LiveLinkBase
{
	GENERATED_BODY()

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FSubjectFrameHandle SubjectFrame;

	UPROPERTY(meta = (Input))
	FName ParameterName;

	UPROPERTY(meta = (Output))
	float Value = 0.f;
};

/**
 * Evaluate current Live Link Transform associated with supplied subject
 */
USTRUCT(meta = (DisplayName = "Evaluate Live Link Frame (Transform)", Category = "Live Link"))
struct LIVELINKCONTROLRIG_API FRigUnit_LiveLinkEvaluteFrameTransform : public FRigUnit_LiveLinkBase
{
	GENERATED_BODY()

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FName SubjectName;

	UPROPERTY(meta = (Input))
	bool bDrawDebug = false;

	UPROPERTY(meta = (Input))
	FLinearColor DebugColor = FLinearColor::Red;

	UPROPERTY(meta = (Input))
	FTransform DebugDrawOffset;

	UPROPERTY(meta = (Output))
	FTransform Transform;
};
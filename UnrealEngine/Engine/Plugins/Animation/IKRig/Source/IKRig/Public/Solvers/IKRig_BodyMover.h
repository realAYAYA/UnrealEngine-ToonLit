// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IKRigDefinition.h"
#include "IKRigSolver.h"

#include "IKRig_BodyMover.generated.h"

UCLASS()
class IKRIG_API UIKRig_BodyMoverEffector : public UObject
{
	GENERATED_BODY()

public:
	UIKRig_BodyMoverEffector() { SetFlags(RF_Transactional); }
	
	UPROPERTY(VisibleAnywhere, Category = "Body Mover Effector")
	FName GoalName;

	UPROPERTY(VisibleAnywhere, Category = "Body Mover Effector")
	FName BoneName;

	/** Scale the influence this effector has on the body. Range is 0-10. Default is 1.0. */
	UPROPERTY(EditAnywhere, Category = "Body Mover Effector", meta = (ClampMin = "0", ClampMax = "10", UIMin = "0.0", UIMax = "10.0"))
	float InfluenceMultiplier = 1.0f;
};

UCLASS(EditInlineNew)
class IKRIG_API UIKRig_BodyMover : public UIKRigSolver
{
	GENERATED_BODY()

public:

	/** The target bone to move with the effectors. */
	UPROPERTY(VisibleAnywhere, Category = "Body Mover Settings")
	FName RootBone;

	/** Blend the translational effect of this solver on/off. Range is 0-1. Default is 1.0. */
	UPROPERTY(EditAnywhere, Category = "Body Mover Settings", meta = (UIMin = "0.0", UIMax = "1.0"))
	float PositionAlpha = 1.0f;

	/** Multiply the POSITIVE X translation. Range is 0-1. Default is 1.0. */
	UPROPERTY(EditAnywhere, Category = "Body Mover Settings", meta = (UIMin = "0.0", UIMax = "1.0"))
	float PositionPositiveX = 1.0f;

	/** Multiply the NEGATIVE X translation. Range is 0-1. Default is 1.0. */
	UPROPERTY(EditAnywhere, Category = "Body Mover Settings", meta = (UIMin = "0.0", UIMax = "1.0"))
	float PositionNegativeX = 1.0f;

	/** Multiply the POSITIVE Y translation. Range is 0-1. Default is 1.0. */
	UPROPERTY(EditAnywhere, Category = "Body Mover Settings", meta = (UIMin = "0.0", UIMax = "1.0"))
	float PositionPositiveY = 1.0f;

	/** Multiply the NEGATIVE Y translation. Range is 0-1. Default is 1.0. */
	UPROPERTY(EditAnywhere, Category = "Body Mover Settings", meta = (UIMin = "0.0", UIMax = "1.0"))
	float PositionNegativeY = 1.0f;

	/** Multiply the POSITIVE Z translation. Range is 0-1. Default is 1.0. */
	UPROPERTY(EditAnywhere, Category = "Body Mover Settings", meta = (UIMin = "0.0", UIMax = "1.0"))
	float PositionPositiveZ = 1.0f;

	/** Multiply the NEGATIVE Z translation. Range is 0-1. Default is 1.0. */
	UPROPERTY(EditAnywhere, Category = "Body Mover Settings", meta = (UIMin = "0.0", UIMax = "1.0"))
	float PositionNegativeZ = 1.0f;

	/** Blend the total rotational effect on/off. Range is 0-1. Default is 1.0. */
	UPROPERTY(EditAnywhere, Category = "Body Mover Settings", meta = (UIMin = "0.0", UIMax = "1.0"))
	float RotationAlpha = 1.0f;

	/** Blend the X-axis rotational effect on/off. Range is 0-1. Default is 1.0. */
	UPROPERTY(EditAnywhere, Category = "Body Mover Settings", meta = (UIMin = "0.0", UIMax = "1.0"))
	float RotateXAlpha = 1.0f;

	/** Blend the Y-axis rotational effect on/off. Range is 0-1. Default is 1.0. */
	UPROPERTY(EditAnywhere, Category = "Body Mover Settings", meta = (UIMin = "0.0", UIMax = "1.0"))
	float RotateYAlpha = 1.0f;

	/** Blend the Z-axis rotational effect on/off. Range is 0-1. Default is 1.0. */
	UPROPERTY(EditAnywhere, Category = "Body Mover Settings", meta = (UIMin = "0.0", UIMax = "1.0"))
	float RotateZAlpha = 1.0f;
	
	UPROPERTY()
	TArray<TObjectPtr<UIKRig_BodyMoverEffector>> Effectors;

	/** UIKRigSolver interface */
	virtual void Initialize(const FIKRigSkeleton& IKRigSkeleton) override;
	virtual void Solve(FIKRigSkeleton& IKRigSkeleton, const FIKRigGoalContainer& Goals) override;

	virtual FName GetRootBone() const override { return RootBone; };
	virtual void RemoveGoal(const FName& GoalName) override;
	virtual void UpdateSolverSettings(UIKRigSolver* InSettings) override;

#if WITH_EDITOR
	virtual FText GetNiceName() const override;
	virtual bool GetWarningMessage(FText& OutWarningMessage) const override;
	virtual bool IsBoneAffectedBySolver(const FName& BoneName, const FIKRigSkeleton& IKRigSkeleton) const override;
	// goals
	virtual void AddGoal(const UIKRigEffectorGoal& NewGoal) override;
	virtual void RenameGoal(const FName& OldName, const FName& NewName) override;
	virtual void SetGoalBone(const FName& GoalName, const FName& NewBoneName) override;
	virtual bool IsGoalConnected(const FName& GoalName) const override;
	virtual UObject* GetGoalSettings(const FName& GoalName) const override;
	// root bone can be set on this solver
	virtual bool RequiresRootBone() const override { return true; };
	virtual void SetRootBone(const FName& RootBoneName) override;
	/** END UIKRigSolver interface */
#endif

private:

	int32 GetIndexOfGoal(const FName& OldName) const;

	int32 BodyBoneIndex;
};


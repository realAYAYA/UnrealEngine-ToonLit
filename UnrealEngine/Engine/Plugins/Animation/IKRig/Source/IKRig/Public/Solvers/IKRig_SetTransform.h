// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IKRigDefinition.h"
#include "IKRigSolver.h"

#include "IKRig_SetTransform.generated.h"

UCLASS()
class IKRIG_API UIKRig_SetTransformEffector : public UObject
{
	GENERATED_BODY()

public:

	/** If true, Goal will drive the translation of the target bone. Default is true. */
	UPROPERTY(EditAnywhere, Category = "Set Transform Effector")
	bool bEnablePosition = true;

	/** If true, Goal will drive the rotation of the target bone. Default is true. */
	UPROPERTY(EditAnywhere, Category = "Set Transform Effector")
	bool bEnableRotation = true;

	/** Blend the effector on/off. Range is 0-1. Default is 1.0. */
	UPROPERTY(EditAnywhere, Category = "Set Transform Effector", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Alpha = 1.0f;
};

UCLASS(EditInlineNew)
class IKRIG_API UIKRig_SetTransform : public UIKRigSolver
{
	GENERATED_BODY()

public:
	
	UIKRig_SetTransform();

	UPROPERTY(VisibleAnywhere, Category = "Set Transform Settings")
	FName Goal;

	UPROPERTY(VisibleAnywhere, Category = "Set Transform Settings")
	FName RootBone;
	
	UPROPERTY()
	TObjectPtr<UIKRig_SetTransformEffector> Effector;

	/** UIKRigSolver interface */
	virtual void Initialize(const FIKRigSkeleton& IKRigSkeleton) override;
	virtual void Solve(FIKRigSkeleton& IKRigSkeleton, const FIKRigGoalContainer& Goals) override;
	virtual void UpdateSolverSettings(UIKRigSolver* InSettings) override;
	virtual void RemoveGoal(const FName& GoalName) override;
#if WITH_EDITOR
	virtual FText GetNiceName() const override;
	virtual bool GetWarningMessage(FText& OutWarningMessage) const override;
	virtual void AddGoal(const UIKRigEffectorGoal& NewGoal) override;
	virtual void RenameGoal(const FName& OldName, const FName& NewName) override;
	virtual void SetGoalBone(const FName& GoalName, const FName& NewBoneName) override;
	virtual bool IsGoalConnected(const FName& GoalName) const override;
	virtual UObject* GetGoalSettings(const FName& GoalName) const override;
	virtual bool IsBoneAffectedBySolver(const FName& BoneName, const FIKRigSkeleton& IKRigSkeleton) const override;
#endif
	/** END UIKRigSolver interface */

private:

	int32 BoneIndex;
};


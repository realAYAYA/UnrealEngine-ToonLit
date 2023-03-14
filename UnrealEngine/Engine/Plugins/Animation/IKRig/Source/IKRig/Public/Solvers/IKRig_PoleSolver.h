// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IKRigDefinition.h"
#include "IKRigSolver.h"

#include "IKRig_PoleSolver.generated.h"

UCLASS()
class IKRIG_API UIKRig_PoleSolverEffector : public UObject
{
	GENERATED_BODY()

public:

	UIKRig_PoleSolverEffector() { SetFlags(RF_Transactional); }
	
	UPROPERTY(VisibleAnywhere, Category = "Pole Solver Effector")
	FName GoalName = NAME_None;

	UPROPERTY(VisibleAnywhere, Category = "Pole Solver Effector")
	FName BoneName = NAME_None;

	/** Blend the effector on/off. Range is 0-1. Default is 1.0. */
	UPROPERTY(EditAnywhere, Category = "Pole Solver Effector", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Alpha = 1.0f;
};

UCLASS(EditInlineNew)
class IKRIG_API UIKRig_PoleSolver : public UIKRigSolver
{
	GENERATED_BODY()

public:

	UIKRig_PoleSolver();
	
	UPROPERTY(VisibleAnywhere, DisplayName = "Root Bone", Category = "Pole Solver Settings")
	FName RootName = NAME_None;

	UPROPERTY(VisibleAnywhere, DisplayName = "End Bone", Category = "Pole Solver Settings")
	FName EndName = NAME_None;
	
	/** UIKRigSolver interface */
	virtual void Initialize(const FIKRigSkeleton& IKRigSkeleton) override;
	virtual void Solve(FIKRigSkeleton& IKRigSkeleton, const FIKRigGoalContainer& Goals) override;
	
	virtual void UpdateSolverSettings(UIKRigSolver* InSettings) override;
	virtual void RemoveGoal(const FName& GoalName) override;

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
	// end bone can be set on this solver
	virtual void SetEndBone(const FName& EndBoneName) override;
	virtual bool RequiresEndBone() const override { return true; };
	/** END UIKRigSolver interface */
#endif

private:

	static void GatherChildren(const int32 BoneIndex, const FIKRigSkeleton& InSkeleton, TArray<int32>& OutChildren);
	
	UPROPERTY()
	TObjectPtr<UIKRig_PoleSolverEffector>	Effector;
	
	TArray<int32>							Chain;
	TArray<int32>							ChildrenToUpdate;
};


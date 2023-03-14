// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IKRigDefinition.h"
#include "IKRigSolver.h"

#include "Solvers/LimbSolver.h"

#include "IKRig_LimbSolver.generated.h"

UCLASS()
class IKRIG_API UIKRig_LimbEffector : public UObject
{
	GENERATED_BODY()

public:
	UIKRig_LimbEffector() { SetFlags(RF_Transactional); }

	UPROPERTY(VisibleAnywhere, Category = "Limb IK Effector")
	FName GoalName = NAME_None;

	UPROPERTY(VisibleAnywhere, Category = "Limb IK Effector")
	FName BoneName = NAME_None;
};

UCLASS(EditInlineNew)
class IKRIG_API UIKRig_LimbSolver : public UIKRigSolver
{
	GENERATED_BODY()

public:

	UIKRig_LimbSolver();
	
	UPROPERTY(VisibleAnywhere, DisplayName = "Root Bone", Category = "Limb IK Settings")
	FName RootName = NAME_None;

	/** Precision (distance to the target) */
	UPROPERTY(EditAnywhere, Category = "Limb IK Settings", meta = (ClampMin = "0.0"))
	float ReachPrecision = 0.01f;
	
	// TWO BONES SETTINGS
	
	/** Hinge Bones Rotation Axis. This is essentially the plane normal for (hip - knee - foot). */
	UPROPERTY(EditAnywhere, Category = "Limb IK Settings|Two Bones")
	TEnumAsByte<EAxis::Type> HingeRotationAxis = EAxis::None;

	// FABRIK SETTINGS

	/** Number of Max Iterations to reach the target */
	UPROPERTY(EditAnywhere, Category = "Limb IK Settings|FABRIK", meta = (ClampMin = "0", UIMin = "0", UIMax = "100"))
	int32 MaxIterations = 12;

	/** Enable/Disable rotational limits */
	UPROPERTY(EditAnywhere, Category = "Limb IK Settings|FABRIK|Limits")
	bool bEnableLimit = false;

	/** Only used if bEnableRotationLimit is enabled. Prevents the leg from folding onto itself,
	* and forces at least this angle between Parent and Child bone. */
	UPROPERTY(EditAnywhere, Category = "Limb IK Settings|FABRIK|Limits", meta = (EditCondition="bEnableLimit", ClampMin = "0.0", ClampMax = "90.0", UIMin = "0.0", UIMax = "90.0"))
	float MinRotationAngle = 15.f;

	/** Pull averaging only has a visual impact when we have more than 2 bones (3 links). */
	UPROPERTY(EditAnywhere, Category = "Limb IK Settings|FABRIK|Pull Averaging")
	bool bAveragePull = true;

	/** Re-position limb to distribute pull: 0 = foot, 0.5 = balanced, 1.f = hip */
	UPROPERTY(EditAnywhere, Category = "Limb IK Settings|FABRIK|Pull Averaging", meta = (ClampMin = "0.0", ClampMax = "1.0",UIMin = "0.0", UIMax = "1.0"))
	float PullDistribution = 0.5f;
	
	/** Move end effector towards target. If we are compressing the chain, limit displacement.*/
	UPROPERTY(EditAnywhere, Category = "Limb IK Settings|FABRIK|Pull Averaging", meta = (ClampMin = "0.0", ClampMax = "1.0",UIMin = "0.0", UIMax = "1.0"))
	float ReachStepAlpha = 0.7f;

	// TWIST SETTINGS
	
	/** Enable Knee Twist correction, by comparing Foot FK with Foot IK orientation. */
	UPROPERTY(EditAnywhere, Category = "Limb IK Settings|Twist")
	bool bEnableTwistCorrection = false;
	
	/** Forward Axis for Foot bone. */
	UPROPERTY(EditAnywhere, Category = "Limb IK Settings|Twist", meta = (EditCondition="bEnableTwistCorrection"))
	TEnumAsByte<EAxis::Type> EndBoneForwardAxis = EAxis::Y;
	
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
#endif
	/** END UIKRigSolver interface */
	
private:

	static void GatherChildren(const int32 BoneIndex, const FIKRigSkeleton& InSkeleton, TArray<int32>& OutChildren);
	
	UPROPERTY()
	TObjectPtr<UIKRig_LimbEffector> Effector;
	
	FLimbSolver						Solver;
	TArray<int32>					ChildrenToUpdate;
};

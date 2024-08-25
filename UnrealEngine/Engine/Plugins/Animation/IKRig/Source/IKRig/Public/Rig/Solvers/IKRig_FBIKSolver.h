// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Rig/IKRigDefinition.h"
#include "IKRigSolver.h"
#include "Core/PBIKSolver.h"
#include "PBIK_Shared.h"

#include "IKRig_FBIKSolver.generated.h"

UCLASS(BlueprintType)
class IKRIG_API UIKRig_FBIKEffector : public UObject
{
	GENERATED_BODY()

public:
	UIKRig_FBIKEffector() { SetFlags(RF_Transactional); }
	
	/** The Goal that is driving this effector's transform. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Full Body IK Effector")
	FName GoalName;
	
	/** The bone that this effector will pull on. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Full Body IK Effector")
	FName BoneName;

	/** Range 0-inf (default is 0). Explicitly set the number of bones up the hierarchy to consider part of this effector's 'chain'.
	* The "chain" of bones is used to apply Preferred Angles, Pull Chain Alpha and Chain "Sub Solves".
	* If left at 0, the solver will attempt to determine the root of the chain by searching up the hierarchy until it finds a branch or another effector, whichever it finds first.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Full Body IK Effector", meta = (ClampMin = "0", UIMin = "0"))
	int32 ChainDepth = 0;

	/** Range 0-1 (default is 1.0). The strength of the effector when pulling the bone towards it's target location.
	* At 0.0, the effector does not pull at all, but the bones between the effector and the root will still slightly resist motion from other effectors.
	* This can thus act as a "stabilizer" for parts of the body that you do not want to behave in a pure FK fashion.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Full Body IK Effector", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0.0", UIMax = "1.0"))
	float StrengthAlpha = 1.0f;

	/** Range 0-1 (default is 1.0). When enabled (greater than 0.0), the solver internally partitions the skeleton into 'chains' which extend
	 * from the effector up the hierarchy by "Chain Depth". If Chain Depth is 0, the chain root is set to the nearest fork in the skeleton.
	* These chains are pre-rotated and translated, as a whole, towards the effector targets.
	* This can improve the results for sparse bone chains, and significantly improve convergence on dense bone chains.
	* But it may cause undesirable results in highly constrained bone chains (like robot arms).
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Full Body IK Effector", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0.0", UIMax = "1.0"))
	float PullChainAlpha = 1.0f;

	/** Range 0-1 (default is 1.0).
	*Blends the effector bone rotation between the rotation of the effector transform (1.0) and the rotation of the input bone (0.0).*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Full Body IK Effector", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0.0", UIMax = "1.0"))
	float PinRotation = 1.0f;

	UPROPERTY(Transient)
	int32 IndexInSolver = -1;

	void CopySettings(const UIKRig_FBIKEffector* Other)
	{
		ChainDepth = Other->ChainDepth;
		StrengthAlpha = Other->StrengthAlpha;
		PullChainAlpha = Other->PullChainAlpha;
		PinRotation = Other->PinRotation;
	}
};

UCLASS(BlueprintType)
class IKRIG_API UIKRig_FBIKBoneSettings : public UObject
{
	GENERATED_BODY()

public:
	
	UIKRig_FBIKBoneSettings()
		: Bone(NAME_None), 
		X(EPBIKLimitType::Free),
		Y(EPBIKLimitType::Free),
		Z(EPBIKLimitType::Free),
		PreferredAngles(FVector::ZeroVector)
	{
		SetFlags(RF_Transactional);
	}

	/** The bone these settings are applied to. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Bone, meta = (Constant, CustomWidget = "BoneName"))
	FName Bone;
	
	/** Range is 0 to 1 (Default is 0). At higher values, the bone will resist rotating (forcing other bones to compensate). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Stiffness, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0.0", UIMax = "1.0"))
	float RotationStiffness = 0.0f;

	/** Range is 0 to 1 (Default is 0). At higher values, the bone will resist translational motion (forcing other bones to compensate). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Stiffness, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0.0", UIMax = "1.0"))
	float PositionStiffness = 0.0f;

	/** Limit the rotation angle of the bone on the X axis.
	 *Free: can rotate freely in this axis.
	 *Limited: rotation is clamped between the min/max angles relative to the Skeletal Mesh reference pose.
	 *Locked: no rotation is allowed in this axis (will remain at reference pose angle). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Limits)
	EPBIKLimitType X;
	/**Range is -180 to 0 (Default is 0). Degrees of rotation in the negative X direction to allow when joint is in "Limited" mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Limits, meta = (ClampMin = "-180", ClampMax = "0", UIMin = "-180.0", UIMax = "0.0"))
	float MinX = 0.0f;
	/**Range is 0 to 180 (Default is 0). Degrees of rotation in the positive X direction to allow when joint is in "Limited" mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Limits, meta = (ClampMin = "0", ClampMax = "180", UIMin = "0.0", UIMax = "180.0"))
	float MaxX = 0.0f;

	/** Limit the rotation angle of the bone on the Y axis.
	*Free: can rotate freely in this axis.
	*Limited: rotation is clamped between the min/max angles relative to the Skeletal Mesh reference pose.
	*Locked: no rotation is allowed in this axis (will remain at input pose angle). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Limits)
	EPBIKLimitType Y;
	/**Range is -180 to 0 (Default is 0). Degrees of rotation in the negative Y direction to allow when joint is in "Limited" mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Limits, meta = (ClampMin = "-180", ClampMax = "0", UIMin = "-180.0", UIMax = "0.0"))
	float MinY = 0.0f;
	/**Range is 0 to 180 (Default is 0). Degrees of rotation in the positive Y direction to allow when joint is in "Limited" mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Limits, meta = (ClampMin = "0", ClampMax = "180", UIMin = "0.0", UIMax = "180.0"))
	float MaxY = 0.0f;

	/** Limit the rotation angle of the bone on the Z axis.
	*Free: can rotate freely in this axis.
	*Limited: rotation is clamped between the min/max angles relative to the Skeletal Mesh reference pose.
	*Locked: no rotation is allowed in this axis (will remain at input pose angle). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Limits)
	EPBIKLimitType Z;
	/**Range is -180 to 0 (Default is 0). Degrees of rotation in the negative Z direction to allow when joint is in "Limited" mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Limits, meta = (ClampMin = "-180", ClampMax = "0", UIMin = "-180.0", UIMax = "0.0"))
	float MinZ = 0.0f;
	/**Range is 0 to 180 (Default is 0). Degrees of rotation in the positive Z direction to allow when joint is in "Limited" mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Limits, meta = (ClampMin = "0", ClampMax = "180", UIMin = "0.0", UIMax = "180.0"))
	float MaxZ = 0.0f;

	/**When true, this bone will "prefer" to rotate in the direction specified by the Preferred Angles when the chain it belongs to is compressed.
	 * Preferred Angles should be the first method used to fix bones that bend in the wrong direction (rather than limits).
	 * The resulting angles can be visualized on their own by temporarily setting the Solver iterations to 0 and moving the effectors.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = PreferredAngles)
	bool bUsePreferredAngles = false;
	/**The local Euler angles (in degrees) used to rotate this bone when the chain it belongs to is squashed.
	 * This happens by moving the effector at the tip of the chain towards the root of the chain.
	 * This can be used to coerce knees and elbows to bend in the anatomically "correct" direction without resorting to limits (which may require more iterations to converge).*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = PreferredAngles)
	FVector PreferredAngles;

	void CopyToCoreStruct(PBIK::FBoneSettings& Settings) const
	{
		Settings.RotationStiffness = RotationStiffness;
		Settings.PositionStiffness = PositionStiffness;
		Settings.X = static_cast<PBIK::ELimitType>(X);
		Settings.MinX = MinX;
		Settings.MaxX = MaxX;
		Settings.Y = static_cast<PBIK::ELimitType>(Y);
		Settings.MinY = MinY;
		Settings.MaxY = MaxY;
		Settings.Z = static_cast<PBIK::ELimitType>(Z);
		Settings.MinZ = MinZ;
		Settings.MaxZ = MaxZ;
		Settings.bUsePreferredAngles = bUsePreferredAngles;
		Settings.PreferredAngles.Pitch = PreferredAngles.Y;
		Settings.PreferredAngles.Yaw = PreferredAngles.Z;
		Settings.PreferredAngles.Roll = PreferredAngles.X;
	}

	void CopySettings(const UIKRig_FBIKBoneSettings* Other)
	{	
		RotationStiffness = Other->RotationStiffness;
		PositionStiffness = Other->PositionStiffness;
		X = Other->X;
		MinX = Other->MinX;
		MaxX = Other->MaxX;
		Y = Other->Y;
		MinY = Other->MinY;
		MaxY = Other->MaxY;
		Z = Other->Z;
		MinZ = Other->MinZ;
		MaxZ = Other->MaxZ;
		bUsePreferredAngles = Other->bUsePreferredAngles;
		PreferredAngles = Other->PreferredAngles;
	}
};

UCLASS(BlueprintType, EditInlineNew, config = Engine, hidecategories = UObject)
class IKRIG_API UIKRigFBIKSolver : public UIKRigSolver
{
	GENERATED_BODY()

public:

	/** All bones above this bone in the hierarchy will be completely ignored by the solver. Typically this is set to
	 * the top-most skinned bone in the Skeletal Mesh (ie Pelvis, Hips etc), NOT the actual root of the entire skeleton.
	 *
	 * If you want to use the solver on a single chain of bones, and NOT translate the chain, ensure that "PinRoot" is
	 * checked on to disable the root from translating to reach the effector goals.*/
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Full Body IK Settings")
	FName RootBone;

	/** High iteration counts can help solve complex joint configurations with competing constraints, but will increase runtime cost. Default is 20. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SolverSettings, meta = (ClampMin = "0", ClampMax = "1000", UIMin = "0.0", UIMax = "200.0"))
	int32 Iterations = 20;

	/** Iterations used for sub-chains defined by the Chain Depth of the effectors. These are solved BEFORE the main iteration pass. Default is 0. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SolverSettings, meta = (ClampMin = "0", ClampMax = "1000", UIMin = "0.0", UIMax = "200.0"))
	int32 SubIterations = 0;

	/** A global mass multiplier; higher values will make the joints more stiff, but require more iterations. Typical range is 0.0 to 10.0. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SolverSettings, meta = (ClampMin = "0", UIMin = "0.0", UIMax = "10.0"))
	float MassMultiplier = 1.0f;

	/** If true, joints will translate to reach the effectors; causing bones to lengthen if necessary. Good for cartoon effects. Default is false. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SolverSettings)
	bool bAllowStretch = false;

	/** (Default is PrePull) Set the behavior of the solver root.
	*Pre Pull: translates and rotates the root (and all children) by the average motion of the stretched effectors to help achieve faster convergence when reaching far.
	*Pin to Input: locks the translation and rotation of the root bone to the input pose. Overrides any bone settings applied to the root. Good for partial-body solves.
	*Free: treats the root bone like any other and allows it to move freely or according to any bone settings applied to it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RootBehavior)
	EPBIKRootBehavior RootBehavior = EPBIKRootBehavior::PrePull;

	/** Settings only applicable when Root Behavior is set to "PrePull". Use these values to adjust the gross movement and orientation of the entire skeleton. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RootBehavior)
	FRootPrePullSettings PrePullRootSettings;

	/** A global multiplier for all Pull Chain Alpha values on all effectors. Range is 0.0 to 1.0. Default is 1.0. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = AdvancedSettings, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0.0", UIMax = "1.0"))
	float PullChainAlpha = 1.0f;

	/** Maximum angle that a joint can be rotated per constraint iteration. Lower this value if the solve is diverging. Range is 0.0 to 180.0. Default is 30. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = AdvancedSettings, meta = (ClampMin = "0", ClampMax = "45", UIMin = "0.0", UIMax = "180.0"))
	float MaxAngle = 30.f;

	/** Pushes constraints beyond their normal amount to speed up convergence. Increasing this may speed up convergence, but at the cost of stability. Range is 1.0 - 2.0. Default is 1.3. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = AdvancedSettings, meta = (ClampMin = "1",  ClampMax = "2", UIMin = "1.0", UIMax = "2.0"))
	float OverRelaxation = 1.3f;
	
	/** DEPRECATED: When true, the solver is reset each tick to start from the current input pose. Default is true.
	 * If false, incoming animated poses are ignored and the solver starts from the results of the previous solve.
	 * In very limited circumstances, it can be beneficial to use the pose from the previous frame.*/
	UPROPERTY()
	bool bStartSolveFromInputPose_DEPRECATED = true;
	
	UPROPERTY()
	TArray<TObjectPtr<UIKRig_FBIKEffector>> Effectors;

	UPROPERTY()
	TArray<TObjectPtr<UIKRig_FBIKBoneSettings>> BoneSettings;

	/** UIKRigSolver interface */
	// runtime
	virtual void Initialize(const FIKRigSkeleton& IKRigSkeleton) override;
	virtual void Solve(FIKRigSkeleton& IKRigSkeleton, const FIKRigGoalContainer& Goals) override;

	virtual FName GetRootBone() const override { return RootBone; };
	virtual void GetBonesWithSettings(TSet<FName>& OutBonesWithSettings) const override;

	virtual void UpdateSolverSettings(UIKRigSolver* InSettings) override;
	virtual void RemoveGoal(const FName& GoalName) override;
	virtual bool IsGoalConnected(const FName& GoalName) const override;
	
#if WITH_EDITOR
	virtual FText GetNiceName() const override;
	virtual bool GetWarningMessage(FText& OutWarningMessage) const override;
	// goals
	virtual void AddGoal(const UIKRigEffectorGoal& NewGoal) override;
	virtual void RenameGoal(const FName& OldName, const FName& NewName) override;
	virtual void SetGoalBone(const FName& GoalName, const FName& NewBoneName) override;
	virtual UObject* GetGoalSettings(const FName& GoalName) const override;
	// bone settings
	virtual void AddBoneSetting(const FName& BoneName) override;
	virtual void RemoveBoneSetting(const FName& BoneName) override;
	virtual UObject* GetBoneSetting(const FName& BoneName) const override;
	virtual bool UsesBoneSettings() const override{ return true;};
	virtual void DrawBoneSettings(const FName& BoneName, const FIKRigSkeleton& IKRigSkeleton, FPrimitiveDrawInterface* PDI) const override;
	// root bone can be set on this solver
	virtual bool RequiresRootBone() const override { return true; };
	virtual void SetRootBone(const FName& RootBoneName) override;
	virtual bool IsBoneAffectedBySolver(const FName& BoneName, const FIKRigSkeleton& IKRigSkeleton) const override;
	/** END UIKRigSolver interface */

	//** UObject */
	virtual void PostLoad() override;
	//** END UObject */
#endif

private:

	FPBIKSolver Solver;

	int32 GetIndexOfGoal(const FName& GoalName) const;
};



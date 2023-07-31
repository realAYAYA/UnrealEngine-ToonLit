// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PBIKBody.h"
#include "PBIKConstraint.h"
#include "PBIKDebug.h"

#include "PBIKSolver.generated.h"

DECLARE_CYCLE_STAT(TEXT("PBIK Solve"), STAT_PBIK_Solve, STATGROUP_Anim);

namespace PBIK
{

static float GLOBAL_UNITS = 100.0f; // (1.0f = meters), (100.0f = centimeters)

// A long tail ease out function. Input range, 0-1. 
FORCEINLINE static float QuarticEaseOut(const float& Input){ return (FMath::Pow(Input-1.0f, 4.0f) * -1.0f) + 1.0f; };
// A C2 continuous ease out function. Input range, 0-1
FORCEINLINE static float CircularEaseOut(const float& Input){ return FMath::Sqrt(1.0f - FMath::Pow(Input - 1.0f, 2.0f)); };
// An ease out function. Input range, 0-1.
FORCEINLINE static float SquaredEaseOut(const float& Input){ return (FMath::Pow(Input-1.0f, 2.0f) * -1.0f) + 1.0f; };

struct FRigidBody;

struct FEffectorSettings
{
	/** Range 0-1, default is 1. Blend between the input bone position (0.0) and the current effector position (1.0).*/
	UPROPERTY(EditAnywhere, Category = "Goal Settings", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float PositionAlpha = 1.0f;

	/** Range 0-1, default is 1. Blend between the input bone rotation (0.0) and the current effector rotation (1.0).*/
	UPROPERTY(EditAnywhere, Category = "Goal Settings", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float RotationAlpha = 1.0f;

	/** Range 0-1 (default is 1.0). The strength of the effector when pulling the bone towards it's target location.
	* At 0.0, the effector does not pull at all, but the bones between the effector and the root will still slightly resist motion from other effectors.
	* This can thus act as a "stabilizer" of sorts for parts of the body that you do not want to behave in a pure FK fashion.
	*/
	UPROPERTY(EditAnywhere, Category="Effector")
	float StrengthAlpha = 1.0f;

	/** Range 0-1 (default is 1.0). When enabled (greater than 0.0), the solver internally partitions the skeleton into 'chains' which extend from the effector to the nearest fork in the skeleton.
	*These chains are pre-rotated and translated, as a whole, towards the effector targets.
	*This can improve the results for sparse bone chains, and significantly improve convergence on dense bone chains.
	*But it may cause undesirable results in highly constrained bone chains (like robot arms).
	*/
	UPROPERTY(EditAnywhere, Category="Effector")
	float PullChainAlpha = 1.0f;

	/** Range 0-1 (default is 1.0).
	*Blends the effector bone rotation between the rotation of the effector transform (1.0) and the rotation of the input bone (0.0).*/
	UPROPERTY(EditAnywhere, Category="Effector")
	float PinRotation = 1.0f;
};

struct FEffector
{
	FVector Position;
	FQuat Rotation;

	FVector PositionOrig;
	FQuat RotationOrig;

	FVector PositionGoal;
	FQuat RotationGoal;

	FEffectorSettings Settings;

	FBone* Bone;
	TWeakPtr<FPinConstraint> Pin;
	FRigidBody* ParentSubRoot = nullptr;
	float DistanceToSubRootInInputPose;
	float DistToRootAlongBones;
	float DistToRootStraightLine;
	
	TArray<float> DistancesFromEffector;
	float DistToSubRootAlongBones;

	FEffector(FBone* InBone);

	void SetGoal(
		const FVector& InPositionGoal,
		const FQuat& InRotationGoal,
		const FEffectorSettings& InSettings);

	void UpdateFromInputs(const FBone& SolverRoot);
	void ApplyPreferredAngles();
};
	
} // namespace

UENUM()
enum class EPBIKRootBehavior : uint8
{
	PrePull,
	PinToInput,
	Free,
};

USTRUCT(BlueprintType)
struct PBIK_API FPBIKSolverSettings
{
	GENERATED_BODY()

	/** High iteration counts can help solve complex joint configurations with competing constraints, but will increase runtime cost. Default is 20. */
	UPROPERTY(EditAnywhere, Category = SolverSettings, meta = (ClampMin = "0", ClampMax = "1000", UIMin = "0.0", UIMax = "200.0"))
	int32 Iterations = 20;

	/** A global mass multiplier; higher values will make the joints more stiff, but require more iterations. Typical range is 0.0 to 10.0. */
	UPROPERTY(EditAnywhere, Category = SolverSettings, meta = (ClampMin = "0", UIMin = "0.0", UIMax = "10.0"))
	float MassMultiplier = 1.0f;

	/** Set this as low as possible while keeping the solve stable. Lower values improve convergence of effector targets. Default is 0.2. */
	UPROPERTY(EditAnywhere, Category = SolverSettings, meta = (ClampMin = "0", UIMin = "0.0", UIMax = "10.0"))
	float MinMassMultiplier = 0.2f;

	/** If true, joints will translate to reach the effectors; causing bones to lengthen if necessary. Good for cartoon effects. Default is false. */
	UPROPERTY(EditAnywhere, Category = SolverSettings)
	bool bAllowStretch = false;

	/** (Default is PrePull) Set the behavior of the solver root.
	*Pre Pull: translates the root (and all children) by the average motion of the stretched effectors to help achieve faster convergence when reaching far.
	*Pin to Input: locks the translation and rotation of the root bone to the input pose. Overrides any bone settings applied to the root. Good for partial-body solves.
	*Free: treats the root bone like any other and allows it to move freely or according to any bone settings applied to it. */
	UPROPERTY(EditAnywhere, Category = SolverSettings)
	EPBIKRootBehavior RootBehavior = EPBIKRootBehavior::PrePull;

	/** When true, the solver is reset each tick to start from the current input pose. If false, incoming animated poses are ignored and the solver starts from the results of the previous solve. Default is true. */
	UPROPERTY(EditAnywhere, Category = SolverSettings)
	bool bStartSolveFromInputPose = true;
};

USTRUCT()
struct PBIK_API FPBIKSolver
{
	GENERATED_BODY()

public:

	PBIK::FDebugDraw* GetDebugDraw();

	//
	// main runtime functions
	//

	bool Initialize();

	void Solve(const FPBIKSolverSettings& Settings);

	void Reset();

	bool IsReadyToSimulate() const;

	//
	// set input / get output at runtime
	//

	void SetBoneTransform(const int32 Index, const FTransform& InTransform);

	PBIK::FBoneSettings* GetBoneSettings(const int32 Index);

	void SetEffectorGoal(
		const int32 Index, 
		const FVector& InPosition, 
		const FQuat& InRotation, 
		const PBIK::FEffectorSettings& Settings);

	void GetBoneGlobalTransform(const int32 Index, FTransform& OutTransform);

	int32 GetNumBones() const { return Bones.Num(); }

	int32 GetBoneIndex(FName BoneName) const;

	//
	// pre-init /  setup functions
	//

	int32 AddBone(
		const FName Name,
		const int32 ParentIndex,
		const FVector& InOrigPosition,
		const FQuat& InOrigRotation,
		bool bIsSolverRoot);

	int32 AddEffector(FName BoneName);
	
private:

	bool InitBones();

	bool InitBodies();

	bool InitConstraints();

	void AddBodyForBone(PBIK::FBone* Bone);

	void UpdateBodies(const FPBIKSolverSettings& Settings);

	void UpdateBonesFromBodies();

	void SolveConstraints(const int32 Iterations, const bool bMoveRoots, const bool bAllowStretch);
	
	void PullRootTowardsEffectors();
	
	void ApplyPullChainAlpha();

	void ApplyPreferredAngles();

private:

	PBIK::FBone* SolverRoot = nullptr;
	TWeakPtr<PBIK::FPinConstraint> RootPin = nullptr;
	TArray<PBIK::FBone> Bones;
	TArray<PBIK::FRigidBody> Bodies;
	TArray<TSharedPtr<PBIK::FConstraint>> Constraints;
	TArray<PBIK::FEffector> Effectors;
	bool bReadyToSimulate = false;
	
	PBIK::FDebugDraw DebugDraw;
	friend PBIK::FDebugDraw;
};
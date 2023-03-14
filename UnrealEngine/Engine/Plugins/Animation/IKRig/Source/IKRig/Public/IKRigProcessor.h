// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IKRigDataTypes.h"
#include "IKRigLogger.h"
#include "IKRigSkeleton.h"

#include "IKRigProcessor.generated.h"

class UIKRigDefinition;
class UIKRigSolver;

USTRUCT()
struct FGoalBone
{
	GENERATED_BODY()
	
	FName BoneName;
	int32 BoneIndex;
	int32 OptSourceIndex = INDEX_NONE;
};

UCLASS()
class IKRIG_API UIKRigProcessor : public UObject
{
	GENERATED_BODY()
	
public:
	
	/** the runtime for an IKRig to convert an input pose into
	*   a solved output pose given a set of IK Rig Goals:
	*   
	* 1. Create a new IKRigProcessor once using MakeNewIKRigProcessor()
	* 2. Initialize() with an IKRigDefinition asset
	* 3. each tick, call SetIKGoal() and SetInputPoseGlobal()
	* 4. Call Solve()
	* 5. Copy output transforms with CopyOutputGlobalPoseToArray()
	* 
	*/

	UIKRigProcessor();
	
	/** setup a new processor to run the given IKRig asset
	 *  NOTE!! this function creates new UObjects and consequently MUST be called from the main thread!!
	 *  @param InRigAsset - the IK Rig defining the collection of solvers to execute and all the rig settings
	 *  @param SkeletalMesh - the skeletal mesh you want to solve the IK on
	 *  @param ExcludedGoals - a list of goal names to exclude from this processor instance (support per-instance removal of goals)
	 */
	void Initialize(
		const UIKRigDefinition* InRigAsset,
		const USkeletalMesh* SkeletalMesh,
		const TArray<FName>& ExcludedGoals = TArray<FName>());

	//
	// BEGIN UPDATE SEQUENCE FUNCTIONS
	//
	// This is the general sequence of function calls to run a typical IK solve:
	//
	
	/** Set all bone transforms in global space. This is the pose the IK solve will start from */
	void SetInputPoseGlobal(const TArray<FTransform>& InGlobalBoneTransforms);

	/** Optionally can be called before Solve() to use the reference pose as start pose */
	void SetInputPoseToRefPose();

	/** Set a named IK goal to go to a specific location, rotation and space, blended by separate position/rotation alpha (0-1)*/
	void SetIKGoal(const FIKRigGoal& Goal);

	/** Set a named IK goal to go to a specific location, rotation and space, blended by separate position/rotation alpha (0-1)*/
	void SetIKGoal(const UIKRigEffectorGoal* Goal);

	/** Run entire stack of solvers.
	 * If any Goals were supplied in World Space, a valid WorldToComponent transform must be provided.  */
	void Solve(const FTransform& WorldToComponent = FTransform::Identity);

	/** Get the results after calling Solve() */
	void CopyOutputGlobalPoseToArray(TArray<FTransform>& OutputPoseGlobal) const;

	/** Reset all internal data structures. Will require re-initialization before solving again. */
	void Reset();

	//
	// END UPDATE SEQUENCE FUNCTIONS
	//

	/** Get access to the internal goal data (read only) */
	const FIKRigGoalContainer& GetGoalContainer() const;
	/** Get the bone associated with a goal */
	const FGoalBone* GetGoalBone(const FName& GoalName) const;
	
	/** Get read/write access to the internal skeleton data */
	FIKRigSkeleton& GetSkeletonWriteable();
	/** Get read-only access to the internal skeleton data */
	const FIKRigSkeleton& GetSkeleton() const;

	/** Used to determine if the IK Rig asset is compatible with a given skeleton. */
	static bool IsIKRigCompatibleWithSkeleton(
		const UIKRigDefinition* InRigAsset,
		const FIKRigInputSkeleton& InputSkeleton,
		const FIKRigLogger* Log);

	bool IsInitialized() const { return bInitialized; };

	void SetNeedsInitialized();

	/** logging system */
	FIKRigLogger Log;

	/** Used to propagate setting values from the source asset at runtime (settings that do not require re-initialization) */
	void CopyAllInputsFromSourceAssetAtRuntime(const UIKRigDefinition* SourceAsset);
	
private:

	/** Update the final pos/rot of all the goals based on their alpha values and their space settings. */
	void ResolveFinalGoalTransforms(const FTransform& WorldToComponent);

	/** the stack of solvers to run in order */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UIKRigSolver>> Solvers;

	/** the named transforms that solvers use as end effectors */
	FIKRigGoalContainer GoalContainer;

	/** map of goal names to bone names/indices */
	TMap<FName, FGoalBone> GoalBones;

	/** storage for hierarchy and bone transforms */
	FIKRigSkeleton Skeleton;

	/** solving disabled until this flag is true */
	bool bInitialized = false;
	bool bTriedToInitialize = false;
};